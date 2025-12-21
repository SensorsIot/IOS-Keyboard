#ifndef CONFIG_H
#define CONFIG_H

// Application version (can be overridden via build flags)
#ifndef CONFIG_APP_VERSION
#define CONFIG_APP_VERSION "2.9.0"
#endif

// Access Point configuration for captive portal
#ifndef CONFIG_AP_SSID
#define CONFIG_AP_SSID "IOS-Keyboard-Setup"
#endif

#ifndef CONFIG_AP_MAX_CONNECTIONS
#define CONFIG_AP_MAX_CONNECTIONS 4
#endif

// AP IP configuration
#define CONFIG_AP_IP          "192.168.4.1"
#define CONFIG_AP_GATEWAY     "192.168.4.1"
#define CONFIG_AP_NETMASK     "255.255.255.0"

// Web server port
#define CONFIG_WEB_SERVER_PORT 80

// Log buffer size for debug interface
#ifndef CONFIG_LOG_BUFFER_SIZE
#define CONFIG_LOG_BUFFER_SIZE 50
#endif

// Keyboard typing delay (ms between keystrokes)
#ifndef CONFIG_TYPING_DELAY_MS
#define CONFIG_TYPING_DELAY_MS 50
#endif

// NVS namespace for WiFi credentials
#define CONFIG_NVS_NAMESPACE "ios_kbd"
#define CONFIG_NVS_KEY_SSID "wifi_ssid"
#define CONFIG_NVS_KEY_PASS "wifi_pass"

// WiFi connection timeout (seconds)
#define CONFIG_WIFI_CONNECT_TIMEOUT_S 10

// WiFi retry settings
#define CONFIG_WIFI_MAX_RETRY 5

// mDNS hostname (optional)
#define CONFIG_MDNS_HOSTNAME "ios-keyboard"

// OTA settings
#define CONFIG_OTA_RECV_TIMEOUT_MS 10000
#define CONFIG_OTA_BUF_SIZE 4096

// Feature flags for phased development
#define CONFIG_ENABLE_HID 1  // USB HID - init after WiFi for safe recovery
#define CONFIG_ENABLE_OTA 1
#define CONFIG_ENABLE_CAPTIVE_PORTAL 1
#define CONFIG_ENABLE_DEBUG_SERVER 1
#define CONFIG_ENABLE_BLE 1  // BLE enabled

// BLE Configuration
#define CONFIG_BLE_DEVICE_NAME "IOS-Keyboard"

// Nordic UART Service (NUS) UUIDs
// Service: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
// RX Char: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E (Write - receive from phone)
// TX Char: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E (Notify - send to phone)

// Command packet types
#define CMD_BACKSPACE 0x01  // 0x01 <count> - send backspace count times
#define CMD_INSERT    0x02  // 0x02 <text>  - type text characters
#define CMD_ENTER     0x03  // 0x03         - send enter key

#endif // CONFIG_H
