#include <WiFi.h>
#include <WebServer.h>
#include "esp_camera.h"
#include "driver/ledc.h"

// WiFi credentials
const char* ssid = "NETGEAR85";
const char* password = "5544332211";

// Camera settings (pins remain the same)
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM     4
#define SIOC_GPIO_NUM     5
#define Y9_GPIO_NUM       16
#define Y8_GPIO_NUM       17
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       12
#define Y5_GPIO_NUM       10
#define Y4_GPIO_NUM       8
#define Y3_GPIO_NUM       9
#define Y2_GPIO_NUM       11
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM     13

// Motor pins
const int motor1In1 = 40;
const int motor1In2 = 39;
const int motor2In1 = 42;
const int motor2In2 = 41;

// PWM settings
#define LEDC_TIMER_RESOLUTION  LEDC_TIMER_8_BIT
#define LEDC_BASE_FREQ        5000
const int maxDuty = 255;
const int halfDuty = 127;

WebServer server(80);
TaskHandle_t streamTaskHandle = NULL;

// Motor channel configuration
ledc_channel_config_t ledc_channel[4];

void setupPWM() {
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_RESOLUTION,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = LEDC_BASE_FREQ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // Configure motor channels
    for (int i = 0; i < 4; i++) {
        ledc_channel[i].channel = (ledc_channel_t)i;
        ledc_channel[i].duty = 0;
        ledc_channel[i].gpio_num = (i == 0) ? motor1In1 : (i == 1) ? motor1In2 : (i == 2) ? motor2In1 : motor2In2;
        ledc_channel[i].speed_mode = LEDC_LOW_SPEED_MODE;
        ledc_channel[i].timer_sel = LEDC_TIMER_0;
        ledc_channel_config(&ledc_channel[i]);
    }
}

void setMotorSpeed(int channel, uint32_t duty) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
}

void stopMotors() {
    for (int i = 0; i < 4; i++) {
        setMotorSpeed(i, 0);
    }
}

void moveForward() {
    setMotorSpeed(0, halfDuty);
    setMotorSpeed(1, 0);
    setMotorSpeed(2, halfDuty);
    setMotorSpeed(3, 0);
}

void moveBackward() {
    setMotorSpeed(0, 0);
    setMotorSpeed(1, halfDuty);
    setMotorSpeed(2, 0);
    setMotorSpeed(3, halfDuty);
}

void turnLeft() {
    setMotorSpeed(0, 0);
    setMotorSpeed(1, halfDuty);
    setMotorSpeed(2, halfDuty);
    setMotorSpeed(3, 0);
}

void turnRight() {
    setMotorSpeed(0, halfDuty);
    setMotorSpeed(1, 0);
    setMotorSpeed(2, 0);
    setMotorSpeed(3, halfDuty);
}

void streamCameraTask(void *pvParameters) {
    WiFiClient client = *((WiFiClient *)pvParameters);
    
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
    client.println();

    while (client.connected()) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed: Frame buffer is null");
        } else {
            Serial.printf("Captured frame: %d bytes\n", fb->len);
        }


        client.print("--frame\r\n");
        client.print("Content-Type: image/jpeg\r\n");
        client.printf("Content-Length: %u\r\n\r\n", fb->len);
        client.write(fb->buf, fb->len);
        client.print("\r\n");

        esp_camera_fb_return(fb);
        
        // Short delay to prevent watchdog triggers
        vTaskDelay(1);
    }
    
    vTaskDelete(NULL);
}

void handleRoot() {
    String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
        <title>Robot Controller</title>
        <style>
            body { font-family: Arial, sans-serif; text-align: center; margin: 20px; }
            .control-panel { display: inline-block; margin: 20px; }
            .video-feed { margin: 20px; }
            button {
                width: 100px;
                height: 50px;
                margin: 5px;
                font-size: 16px;
                cursor: pointer;
            }
            #status { margin: 20px; color: #666; }
        </style>
    </head>
    <body>
        <h1>Robot Controls</h1>
        <div class="video-feed">
            <img src="/stream" style="width: 640px; height: 480px;">
        </div>
        <div class="control-panel">
            <div><button onmousedown="control('forward')" onmouseup="control('stop')">Forward</button></div>
            <div>
                <button onmousedown="control('left')" onmouseup="control('stop')">Left</button>
                <button onclick="control('stop')" style="background-color: #ff4444;">STOP</button>
                <button onmousedown="control('right')" onmouseup="control('stop')">Right</button>
            </div>
            <div><button onmousedown="control('backward')" onmouseup="control('stop')">Backward</button></div>
        </div>
        <div id="status">Status: Ready</div>
        
        <script>
            function control(command) {
                fetch('/control?cmd=' + command, {
                    method: 'GET',
                    cache: 'no-cache',
                }).catch(console.error);
            }
        </script>
    </body>
    </html>
    )rawliteral";
    server.send(200, "text/html", html);
}

void handleControl() {
    String cmd = server.arg("cmd");
    server.send(200, "text/plain", "OK");
    
    if (cmd == "forward") moveForward();
    else if (cmd == "backward") moveBackward();
    else if (cmd == "left") turnLeft();
    else if (cmd == "right") turnRight();
    else stopMotors();
}

void handleStream() {
    WiFiClient* client = new WiFiClient(server.client());
    
    // Create a task for video streaming on core 0
    xTaskCreatePinnedToCore(
        streamCameraTask,    // Function to implement the task
        "streamTask",        // Name of the task
        8192,               // Stack size in words
        client,             // Task input parameter
        2,                  // Priority of the task
        &streamTaskHandle,  // Task handle
        0                   // Core where the task should run
    );
}

void setup() {
  if (!psramInit()) {
    Serial.println("PSRAM initialization failed");
    return;
}

    Serial.begin(115200);
    
    // Initialize PWM and motors
    setupPWM();
    stopMotors();
    
    // Configure camera
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_4;
    config.ledc_timer = LEDC_TIMER_1;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 15;  // Lower quality for better performance
    config.fb_count = 1;

    // Initialize camera
    if (!psramInit()) {
        Serial.println("PSRAM initialization failed");
        return;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
    }
        // Get the sensor
    sensor_t * s = esp_camera_sensor_get();
    if (s) {
        // Flip the image
        s->set_hmirror(s, 1);   // 1 = flip horizontal
        s->set_vflip(s, 1);     // 1 = flip vertical
        
        // Additional settings for better FPS
        s->set_brightness(s, 1);     // -2,2 (default 0)
        s->set_contrast(s, 1);       // -2,2 (default 0)
        s->set_saturation(s, 1);     // -2,2 (default 0)
        s->set_special_effect(s, 0); // 0 = no effect
        s->set_whitebal(s, 1);       // 1 = enable auto white balance
        s->set_awb_gain(s, 1);       // 1 = enable auto white balance gain
        s->set_wb_mode(s, 0);        // 0 = auto mode
        s->set_exposure_ctrl(s, 1);   // 1 = enable auto exposure
        s->set_aec2(s, 1);           // 1 = enable auto exposure (sensor method)
        s->set_ae_level(s, 0);       // -2,2 (default 0)
        s->set_aec_value(s, 300);    // 0-1200 (default 300)
        s->set_gain_ctrl(s, 1);      // 1 = enable auto gain
        s->set_agc_gain(s, 0);       // 0-30
        s->set_gainceiling(s, (gainceiling_t)6); // 0-6
        s->set_bpc(s, 1);            // 1 = enable black pixel correction
        s->set_wpc(s, 1);            // 1 = enable white pixel correction
        s->set_raw_gma(s, 1);        // 1 = enable gamma correction
        s->set_lenc(s, 1);           // 1 = enable lens correction
        s->set_dcw(s, 1);            // 1 = enable downsize enable
        s->set_colorbar(s, 0);       // 0 = disable test pattern
    }
    
    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    Serial.println("IP address: " + WiFi.localIP().toString());
    
    // Setup server routes
    server.on("/", HTTP_GET, handleRoot);
    server.on("/control", HTTP_GET, handleControl);
    server.on("/stream", HTTP_GET, handleStream);
    
    server.begin();
}

void loop() {
    server.handleClient();
    delay(1);  // Prevent watchdog timeouts
}