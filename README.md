# ESP32 WiFi Robot Controller

## Overview
This project creates a web-controlled robot using an ESP32 with a camera, capable of streaming video and accepting remote movement commands via a web interface.

## Features
- Real-time video streaming
- Mobile and desktop compatible control interface
- Movement controls: Forward, Backward, Left, Right, Stop
- Touch and mouse event support

## Hardware Requirements
- ESP32 with camera module
- 2-wheel drive motor setup
- WiFi connectivity

## Software Dependencies
- Arduino IDE
- ESP32 Board Support Package
- ESP Camera Library
- WebServer Library

## Setup Instructions
1. Install required libraries in Arduino IDE
2. Configure WiFi credentials in code
3. Set correct GPIO pin mappings for your specific hardware
4. Upload code to ESP32

## Web Interface
- Access robot controls via device's IP address
- Touch or click buttons to control robot movement
- Real-time camera stream displayed on page

## Customization
- Adjust motor speed by modifying `halfDuty` constant
- Configure camera settings in `setup()` function
- Modify GPIO pins to match your specific hardware configuration

## Troubleshooting
- Ensure correct WiFi credentials
- Verify motor and camera pin connections
- Check ESP32 board compatibility

## Safety Notes
- Always have a manual stop mechanism
- Supervise robot during operation
- Ensure clear operating environment
