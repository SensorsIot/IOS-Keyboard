#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"

// WiFi manager mode (prefixed to avoid conflict with ESP-IDF wifi_mode_t)
typedef enum {
    WIFI_MGR_MODE_NONE,
    WIFI_MGR_MODE_AP,      // Access Point mode (captive portal)
    WIFI_MGR_MODE_STA,     // Station mode (connected to network)
} wifi_manager_mode_t;

// WiFi status
typedef struct {
    wifi_manager_mode_t mode;
    bool connected;
    char ssid[33];
    int8_t rssi;
    char ip_addr[16];
} wifi_manager_status_t;

/**
 * Initialize the WiFi manager
 * Initializes NVS, WiFi, and event handlers
 */
esp_err_t wifi_manager_init(void);

/**
 * Start WiFi based on stored credentials
 * If credentials exist, tries STA mode; otherwise starts AP mode
 */
esp_err_t wifi_manager_start(void);

/**
 * Start Access Point mode for captive portal
 */
esp_err_t wifi_manager_start_ap(void);

/**
 * Start Station mode and connect to stored network
 */
esp_err_t wifi_manager_start_sta(void);

/**
 * Check if WiFi credentials are stored in NVS
 */
bool wifi_manager_has_credentials(void);

/**
 * Save WiFi credentials to NVS
 */
esp_err_t wifi_manager_save_credentials(const char *ssid, const char *password);

/**
 * Clear stored WiFi credentials from NVS
 */
esp_err_t wifi_manager_clear_credentials(void);

/**
 * Get current WiFi status
 */
wifi_manager_status_t wifi_manager_get_status(void);

/**
 * Scan for available WiFi networks
 * Returns number of networks found, populates results array
 */
int wifi_manager_scan(char results[][33], int8_t rssi[], int max_results);

/**
 * Attempt to connect with new credentials (without saving)
 * Used for testing credentials before saving
 */
esp_err_t wifi_manager_try_connect(const char *ssid, const char *password);

#endif // WIFI_MANAGER_H
