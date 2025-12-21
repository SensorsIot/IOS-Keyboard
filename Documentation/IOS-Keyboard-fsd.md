# IOS-Keyboard Functional Specification Document

**Version:** 1.1
**Date:** 2025-12-21
**Status:** Draft

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
- **Microcontroller:** ESP32-C3 (RISC-V, with native USB support)
- **USB Connection:** Native USB port used for HID keyboard emulation
- **WiFi:** Built-in 2.4GHz WiFi for OTA updates

### 2.2 Software Stack
```
+------------------+
|   Application    |  <- Keyboard logic, OTA handler
+------------------+
|    TinyUSB HID   |  <- USB keyboard emulation
+------------------+
|     ESP-IDF      |  <- WiFi, OTA, system functions
+------------------+
|   ESP32-C3 HW    |
+------------------+
```

### 2.3 Development Environment
- **Framework:** ESP-IDF (via PlatformIO)
- **Build System:** PlatformIO
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
| FR-USB-05 | Device shall support standard US keyboard layout | Must |

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

### 3.6 Trigger Mechanism

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-TRIG-01 | Device shall have a configurable trigger for keyboard output | Must |
| FR-TRIG-02 | Initial trigger: type "hello world" on device boot | Must |

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
| `/reset-wifi` | POST | Clear WiFi credentials, reboot to AP mode |

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

---

## 7. Project Structure

```
IOS-Keyboard/
├── Documentation/
│   └── IOS-Keyboard-fsd.md
├── src/
│   ├── main.c              # Application entry point
│   ├── usb_hid.c           # USB HID keyboard implementation
│   ├── usb_hid.h
│   ├── wifi_manager.c      # WiFi connection handling
│   ├── wifi_manager.h
│   ├── captive_portal.c    # Captive portal web server
│   ├── captive_portal.h
│   ├── debug_server.c      # WiFi debug web server
│   ├── debug_server.h
│   ├── ota_handler.c       # OTA update logic
│   └── ota_handler.h
├── include/
│   └── config.h            # Configuration defines
├── data/
│   └── www/                # Embedded HTML/CSS/JS for web interfaces
│       ├── index.html      # Captive portal page
│       └── debug.html      # Debug dashboard page
├── platformio.ini          # PlatformIO configuration
└── partitions.csv          # Custom partition table for OTA
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
