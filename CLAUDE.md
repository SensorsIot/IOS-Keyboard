# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-based USB HID keyboard emulator with WiFi configuration and OTA updates. Target hardware is ESP32-C3/S3 with native USB.

## Architecture

```
+------------------+
|   Application    |  main.c - entry point, state machine
+------------------+
|  WiFi + Web      |  wifi_manager, captive_portal, debug_server
+------------------+
|   OTA Handler    |  ota_handler - firmware updates via HTTP
+------------------+
|    TinyUSB HID   |  USB keyboard emulation (Phase 2)
+------------------+
|     ESP-IDF      |
+------------------+
```

## Modules

- **wifi_manager** - WiFi AP/STA mode switching, NVS credential storage, connection management
- **captive_portal** - AP mode web server for WiFi configuration at 192.168.4.1
- **debug_server** - STA mode web server for logs, status, OTA trigger, accessible via mDNS
- **ota_handler** - HTTP OTA with rollback support
- **config.h** - All configuration defines and feature flags

## State Flow

1. Boot → Check NVS for WiFi credentials
2. No credentials → AP mode + captive portal
3. Has credentials → STA mode → connect → debug server
4. Connection failure → fallback to AP mode

## Current Phase

Phase 1: OTA testing infrastructure. HID keyboard disabled (`CONFIG_ENABLE_HID=0`).

## Web Endpoints

**Captive Portal (AP mode):** `/`, `/scan`, `/connect`, `/status`

**Debug Server (STA mode):** `/`, `/status`, `/logs`, `/ota`, `/type`, `/reset-wifi`

## Build and Flash

```bash
# Set up ESP-IDF environment
source ~/esp/esp-idf/export.sh

# Set target (first time only)
idf.py set-target esp32c3

# Build
idf.py build

# Flash
idf.py -p /dev/ttyACM0 flash
```

## Serial Monitor

```bash
# Using idf.py (interactive terminal required)
idf.py -p /dev/ttyACM0 monitor

# Using Python (works without TTY)
python3 << 'EOF'
import serial, time
ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
ser.setDTR(False); ser.setRTS(True); time.sleep(0.1); ser.setRTS(False)
start = time.time()
while time.time() - start < 10:
    data = ser.read(1024)
    if data: print(data.decode('utf-8', errors='replace'), end='')
ser.close()
EOF
```

## OTA Update

```bash
# 1. Build new firmware (update CONFIG_APP_VERSION in config.h first)
idf.py build

# 2. Start HTTP server
cd build && python3 -m http.server 8080 &

# 3. Trigger OTA (replace IP with your server's IP)
curl -X POST "http://<device-ip>/ota" \
  -H "Content-Type: application/json" \
  -d '{"url":"http://<server-ip>:8080/IOS-Keyboard.bin"}'

# 4. Check status
curl "http://<device-ip>/status"
```
