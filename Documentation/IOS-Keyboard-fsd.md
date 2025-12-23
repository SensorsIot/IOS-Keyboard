# IOS-Keyboard Functional Specification Document

**Version:** 2.11.0
**Date:** 2025-12-23
**Status:** Phase 5 Complete - Project Restructured

---

## 1. Overview

### 1.1 Purpose
This document specifies the functional requirements for an ESP32-C3 based USB keyboard emulator. The device connects to a host PC via USB and emulates keyboard input, initially typing "hello world" as a proof of concept.

### 1.2 Scope
- ESP32-C3 microcontroller acting as a USB HID keyboard
- Over-The-Air (OTA) firmware update capability
- Development using PlatformIO with ESP-IDF framework

### 1.3 Definitions
| Term | Definition |
|------|------------|
| HID | Human Interface Device (USB device class for keyboards, mice, etc.) |
| OTA | Over-The-Air firmware updates via WiFi |
| TinyUSB | USB stack library for embedded devices |

---

## 2. System Architecture

### 2.1 Hardware
- **Microcontroller:** ESP32-C3 or ESP32-S3 (both support native USB + BLE)
- **USB Connection:** Native USB port used for HID keyboard emulation
- **WiFi:** Built-in 2.4GHz WiFi for OTA updates and debug interface
- **BLE:** Bluetooth Low Energy for iPhone communication

> **Note:** ESP32-C3 (RISC-V) and ESP32-S3 (Xtensa) both support native USB and BLE. The S3 has more RAM/flash and dual-core, but C3 is sufficient for this application.

### 2.2 Software Stack
```
+------------------+
|   Application    |  <- State machine, command dispatch
+------------------+
|    BLE GATT      |  <- iPhone communication (UART service)
+------------------+
|  WiFi + Web      |  <- Captive portal, debug server, OTA
+------------------+
|    TinyUSB HID   |  <- USB keyboard emulation
+------------------+
|     ESP-IDF      |  <- System functions, NVS, drivers
+------------------+
|  ESP32-C3/S3 HW  |
+------------------+
```

### 2.3 Development Environment
- **Framework:** ESP-IDF v6.1
- **Build System:** CMake/Ninja via idf.py
- **USB Stack:** TinyUSB (integrated with ESP-IDF)

---

## 3. Functional Requirements

### 3.1 USB HID Keyboard Emulation

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-USB-01 | Device shall enumerate as a USB HID keyboard when connected to host | Must |
| FR-USB-02 | Device shall be recognized by the host OS without additional drivers | Must |
| FR-USB-03 | Device shall send keyboard HID reports to type characters | Must |
| FR-USB-04 | Device shall type "hello world" as initial functionality | Must |
| FR-USB-05 | Device shall support multiple keyboard layouts (US, Swiss German, German, French, UK, Spanish, Italian) | Must |
| FR-USB-06 | Device shall persist selected keyboard layout to NVS | Must |
| FR-USB-07 | Device shall provide API endpoint to list/select keyboard layouts | Must |

### 3.2 OTA Firmware Updates

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-OTA-01 | Device shall connect to a configured WiFi network | Must |
| FR-OTA-02 | Device shall support OTA firmware updates via HTTP | Must |
| FR-OTA-03 | Device shall validate firmware integrity before applying update | Must |
| FR-OTA-04 | Device shall rollback to previous firmware on update failure | Should |
| FR-OTA-05 | Device shall indicate OTA update status (via serial log or LED) | Should |

### 3.3 WiFi Connectivity

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-WIFI-01 | Device shall store WiFi credentials in non-volatile storage (NVS) | Must |
| FR-WIFI-02 | Device shall automatically reconnect on WiFi disconnection | Should |
| FR-WIFI-03 | Device shall start as Access Point if no WiFi credentials stored | Must |
| FR-WIFI-04 | Device shall provide captive portal for WiFi credential configuration | Must |
| FR-WIFI-05 | Device shall switch from AP mode to Station mode after credentials saved | Must |
| FR-WIFI-06 | Device shall allow re-entering AP/captive portal mode via GPIO button | Should |

### 3.4 Captive Portal

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-CP-01 | Device shall create AP with SSID "IOS-Keyboard-Setup" | Must |
| FR-CP-02 | Device shall serve captive portal on IP 192.168.4.1 | Must |
| FR-CP-03 | Captive portal shall display web form for SSID and password entry | Must |
| FR-CP-04 | Captive portal shall scan and display available WiFi networks | Should |
| FR-CP-05 | Captive portal shall validate credentials before saving | Should |
| FR-CP-06 | Captive portal shall display connection status/errors | Should |

### 3.5 WiFi Debugging

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-DBG-01 | Device shall host a debug web server when WiFi connected | Must |
| FR-DBG-02 | Debug interface shall display device status and logs | Must |
| FR-DBG-03 | Debug interface shall allow manual OTA update trigger | Must |
| FR-DBG-04 | Debug interface shall display current firmware version | Must |
| FR-DBG-05 | Debug interface shall show WiFi signal strength (RSSI) | Should |
| FR-DBG-06 | Debug interface shall allow triggering keyboard output | Should |
| FR-DBG-07 | Device shall buffer last N log messages for web display | Should |
| FR-DBG-08 | Debug interface shall be accessible via mDNS (ios-keyboard.local) | Could |
| FR-DBG-09 | Debug interface shall display BLE-received data in real-time trace view | Should |
| FR-DBG-10 | Debug interface shall display HID-sent keycodes in real-time trace view | Should |

### 3.6 Trigger Mechanism

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-TRIG-01 | Device shall have a configurable trigger for keyboard output | Must |
| FR-TRIG-02 | Initial trigger: type "hello world" on device boot | Must |

### 3.7 BLE Command Interface

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-BLE-01 | Device shall act as BLE peripheral with UART-like service | Must |
| FR-BLE-02 | Device shall advertise and accept connections from iPhone app | Must |
| FR-BLE-03 | Device shall expose a write characteristic for receiving commands | Must |
| FR-BLE-04 | Device shall parse binary command packets from BLE | Must |
| FR-BLE-05 | Command `0x01 <count>` shall send `<count>` backspace keystrokes | Must |
| FR-BLE-06 | Command `0x02 <text>` shall type the text characters via HID | Must |
| FR-BLE-07 | Command `0x03` shall send Enter key | Must |
| FR-BLE-08 | Command `0x04 <key>` shall send Ctrl+key combo (e.g., Ctrl+J) | Must |
| FR-BLE-09 | Device shall handle malformed packets gracefully | Should |

### 3.8 iOS App Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-IOS-01 | App shall scan for BLE devices advertising NUS service or named "IOS-Keyboard" | Must |
| FR-IOS-02 | App shall auto-connect if only one IOS-Keyboard device is found | Must |
| FR-IOS-03 | App shall auto-reconnect if connection is lost | Must |
| FR-IOS-04 | App shall prevent screen from sleeping while active | Must |
| FR-IOS-05 | App shall use speech recognition to convert voice to text | Must |
| FR-IOS-06 | App shall compute minimal text diffs and send only changes via BLE | Must |
| FR-IOS-07 | App shall clear display when recording stops (without deleting text on target) | Must |
| FR-IOS-08 | App shall support magic words that trigger special key combinations | Must |
| FR-IOS-09 | Magic word "Abrahadabra" shall send Ctrl+J (newline in Claude prompt) | Must |
| FR-IOS-10 | App shall display recognized and transmitted text in real-time | Should |
| FR-IOS-11 | App shall show simple "Connecting..." view when auto-connecting (no device list) | Must |
| FR-IOS-12 | App shall show device list only when multiple devices found or no devices after scan | Must |
| FR-IOS-13 | App shall ignore pending transcript updates when stopping to prevent retransmission | Must |
| FR-IOS-14 | App shall have a microphone app icon | Should |
| FR-IOS-15 | App shall reset speech recognition after 10 seconds of silence to start fresh text | Must |
| FR-IOS-16 | App shall log text diff operations for debugging (old text, new text, backspaces, insert) | Should |

---

## 4. System States

```
                         +-------------+
                         |    BOOT     |
                         +------+------+
                                |
                                v
                         +------+------+
                         |  INIT USB   |
                         | (HID Ready) |
                         +------+------+
                                |
                                v
                    +-----------+-----------+
                    | Check WiFi Credentials|
                    +-----------+-----------+
                                |
              +-----------------+-----------------+
              | No credentials          Has credentials
              v                                  v
       +------+------+                   +-------+-------+
       |   AP MODE   |                   |  STA MODE     |
       | Captive     |                   |  Connecting   |
       | Portal      |                   +-------+-------+
       +------+------+                           |
              |                                  v
              | Credentials              +-------+-------+
              | Saved                    | WIFI CONNECTED|
              |                          | Debug Server  |
              v                          | OTA Ready     |
       +------+------+                   +-------+-------+
       |   REBOOT    |                           |
       +-------------+                           v
                                         +-------+-------+
                                         | TYPE TEXT     |
                                         | "hello world" |
                                         +---------------+
```

---

## 5. Interface Specifications

### 5.1 USB HID Interface
- **USB Class:** HID (Human Interface Device)
- **USB Subclass:** Boot Interface
- **Protocol:** Keyboard
- **Report Descriptor:** Standard 8-byte keyboard report

### 5.2 OTA HTTP Interface
- **Method:** GET request to firmware binary URL
- **Endpoint:** Configurable via build-time define
- **Response:** Binary firmware file (.bin)

### 5.3 Captive Portal Interface
- **AP SSID:** IOS-Keyboard-Setup
- **AP IP Address:** 192.168.4.1
- **Web Server Port:** 80

**Endpoints:**
| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Main configuration page with WiFi form |
| `/scan` | GET | Returns JSON list of available networks |
| `/connect` | POST | Submit WiFi credentials (ssid, password) |
| `/status` | GET | Returns connection status |

### 5.4 WiFi Debug Interface
Since Serial/UART is unavailable (USB used for HID), debugging is done via WiFi web interface.

- **Server Port:** 80 (same as captive portal, different mode)
- **mDNS Name:** ios-keyboard.local (optional)

**Endpoints:**
| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Debug dashboard (status, logs, actions) |
| `/status` | GET | JSON device status (version, uptime, RSSI) |
| `/logs` | GET | Returns buffered log messages |
| `/ota` | POST | Trigger OTA update from configured URL |
| `/ota` | GET | OTA status page |
| `/type` | POST | Trigger keyboard output manually |
| `/keyboard` | GET | Get current layout and list available layouts |
| `/keyboard` | POST | Set keyboard layout (JSON: `{"layout":"ch-de"}`) |
| `/reset-wifi` | POST | Clear WiFi credentials, reboot to AP mode |
| `/trace` | GET | Returns BLE and HID trace buffers (JSON: `{"ble":[...], "hid":[...]}`) |

### 5.5 BLE GATT Interface

The ESP32 acts as a BLE peripheral exposing a Nordic UART Service (NUS) compatible interface.

- **Service UUID:** `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
- **RX Characteristic:** `6E400002-B5A3-F393-E0A9-E50E24DCCA9E` (Write)
- **TX Characteristic:** `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` (Notify)

**Command Packet Format:**
| Byte 0 | Bytes 1-N | Description |
|--------|-----------|-------------|
| `0x01` | `<count>` | Send `count` backspace keystrokes |
| `0x02` | `<text>` | Type ASCII/UTF-8 text characters |
| `0x03` | (none) | Send Enter key |
| `0x04` | `<key>` | Send Ctrl+key combo (ASCII value of key) |

**Example Packets:**
- `01 05` → Send 5 backspaces
- `02 48 65 6C 6C 6F` → Type "Hello"
- `03` → Send Enter
- `04 4A` → Send Ctrl+J (newline in Claude prompt)

---

## 6. Configuration

### 6.1 Build-time Configuration
| Parameter | Description | Default |
|-----------|-------------|---------|
| `OTA_URL` | Firmware update URL | (optional) |
| `TYPING_DELAY_MS` | Delay between keystrokes | 50 |
| `LOG_BUFFER_SIZE` | Number of log messages to buffer | 100 |
| `AP_SSID` | Captive portal AP name | IOS-Keyboard-Setup |

### 6.2 Runtime Configuration (NVS)
| Parameter | Storage | Description |
|-----------|---------|-------------|
| WiFi SSID | NVS | Configured via captive portal |
| WiFi Password | NVS | Configured via captive portal |
| Keyboard Layout | NVS | Selected via debug web UI (default: Swiss German) |

---

## 7. Project Structure

```
IOS-Keyboard/
├── Documentation/
│   └── IOS-Keyboard-fsd.md     # This document
├── esp32/                       # ESP32 firmware
│   ├── CMakeLists.txt          # Project CMake configuration
│   ├── CLAUDE.md               # Claude Code guidance
│   ├── main/
│   │   ├── CMakeLists.txt      # Component CMake
│   │   ├── idf_component.yml   # Component dependencies (mdns, cjson, bt)
│   │   ├── config.h            # Configuration defines and feature flags
│   │   ├── main.c              # Application entry point, state machine
│   │   ├── wifi_manager.c/h    # WiFi AP/STA mode, NVS credentials
│   │   ├── captive_portal.c/h  # AP mode web server
│   │   ├── debug_server.c/h    # STA mode debug web server
│   │   ├── ota_handler.c/h     # HTTP OTA with rollback
│   │   ├── ble_gatt.c/h        # BLE peripheral, NUS service
│   │   ├── command_parser.c/h  # Parse binary command packets
│   │   ├── usb_hid.c/h         # USB HID keyboard functions
│   │   └── keyboard_layout.c/h # Multi-keyboard layout support
│   ├── partitions.csv          # Custom partition table for OTA
│   └── sdkconfig.defaults      # Default Kconfig settings
└── ios/                         # iOS App
    ├── IOS-Keyboard-App.xcodeproj/
    └── IOS-Keyboard-App/
        ├── IOS_Keyboard_AppApp.swift   # App entry, idle timer disabled
        ├── ContentView.swift           # Main UI with voice interface
        ├── Info.plist                  # BLE, microphone, speech permissions
        ├── Assets.xcassets/            # App icons and assets
        ├── Services/
        │   ├── BluetoothService.swift  # BLE scanning, auto-connect/reconnect
        │   ├── SpeechRecognitionService.swift  # Voice-to-text
        │   └── TextDiffService.swift   # Minimal diff computation
        ├── ViewModels/
        │   └── MainViewModel.swift     # App logic, magic word handling
        └── Views/
            └── DeviceListView.swift    # BLE device list UI
```

---

## 8. Open Questions / Decisions Needed

| # | Question | Options | Decision |
|---|----------|---------|----------|
| 1 | Target ESP32-C3 board variant | DevKitM-1, XIAO, Custom | TBD |
| 2 | LED status indication | Use onboard LED, External LED, None | TBD |
| 3 | OTA trigger mechanism | Manual via web, Periodic check, On boot | TBD |
| 4 | GPIO pin for reset-to-AP button | GPIO9, GPIO8, Other | TBD |

---

## 9. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2025-12-21 | - | Initial draft |
| 1.1 | 2025-12-21 | - | Added WiFi debugging requirements |
| 1.2 | 2025-12-21 | - | Phase 1 complete: OTA tested, updated to ESP-IDF native build |
| 1.3 | 2025-12-21 | - | Merged BLE GATT interface from esp32_functional_description.md; added command protocol, NUS service spec; updated architecture for ESP32-C3/S3 |
| 1.4 | 2025-12-21 | - | Phase 2 HID working: USB keyboard types via /type endpoint; fixed TinyUSB configuration; BLE pending |
| 1.5 | 2025-12-21 | - | Phase 3 complete: BLE GATT with NimBLE working; Nordic UART Service for iOS app; command protocol (backspace/insert/enter) functional |
| 1.6 | 2025-12-21 | - | Phase 4 complete: Multi-keyboard layout support (US, Swiss German, German, French, UK, Spanish, Italian); layout persists to NVS; selectable via web UI dropdown |
| 1.7 | 2025-12-21 | - | Phase 5 complete: iOS app with Xcode project; auto-scan/connect/reconnect; screen stays on; magic word "Abrahadabra" sends Ctrl+J; clear display on stop; added Ctrl+key command (0x04) |
| 1.8 | 2025-12-21 | - | iOS app polish: Simple "Connecting..." view (no device list for single device); fix retransmission bug on stop; microphone app icon |
| 1.9 | 2025-12-21 | - | Debug trace feature: Added BLE→HID trace view in debug web interface showing received commands and sent keycodes in real-time |
| 2.10.0 | 2025-12-22 | - | Project restructured to esp32/ and ios/ folders; updated documentation |
| 2.11.0 | 2025-12-23 | - | Silence detection: Reset speech recognition after 10s of no transcript updates to prevent text repetition; added diff logging for debugging |
