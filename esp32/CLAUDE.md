# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-S3 USB HID keyboard emulator with WiFi configuration and OTA updates.

## Build Commands

```bash
source ~/esp/esp-idf/export.sh
idf.py set-target esp32s3    # First time only
idf.py build
idf.py -p /dev/ttyACM0 flash
```

## Key Files

- `main/config.h` - Feature flags and configuration defines
- `main/main.c` - Entry point, initialization order
- `main/usb_hid.c` - TinyUSB HID keyboard implementation
- `main/ble_gatt.c` - BLE GATT server (NimBLE)
- `main/debug_server.c` - Web API endpoints
- `main/keyboard_layout.c` - Keyboard layout support (US, Swiss German, German, French, UK, Spanish, Italian)
- `sdkconfig.defaults` - ESP-IDF build configuration

## Initialization Order

Critical order for debugging to work:

1. OTA handler
2. WiFi manager
3. mDNS
4. Debug server (WiFi debugging available from here)
5. HID init
6. BLE init (last)

## Current Status

- v2.8.0: HID + BLE + Keyboard Layouts
- BLE advertises as "IOS-Keyboard" with Nordic UART Service
- Commands via BLE: `0x02`+text (type), `0x01`+count (backspace), `0x03` (enter)
- Keyboard layout selectable via web UI dropdown (persists to NVS)
- API: `GET /keyboard` (list layouts), `POST /keyboard {"layout":"ch-de"}` (set layout)

## OTA Testing

```bash
cd build && python3 -m http.server 8080 &
curl -X POST "http://<device-ip>/ota" \
  -H "Content-Type: application/json" \
  -d '{"url":"http://<server-ip>:8080/IOS-Keyboard.bin"}'
```

## Notes

- HID init must happen AFTER WiFi so portal works for recovery
- BLE init must happen LAST so WiFi/HID work if BLE fails
- See `Documentation/IOS-Keyboard-fsd.md` for functional specification
