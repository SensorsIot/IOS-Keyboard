#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

#include <stdbool.h>
#include "esp_err.h"

/**
 * Start the captive portal web server
 * Serves WiFi configuration page on AP mode
 */
esp_err_t captive_portal_start(void);

/**
 * Stop the captive portal web server
 */
esp_err_t captive_portal_stop(void);

/**
 * Check if captive portal is running
 */
bool captive_portal_is_running(void);

#endif // CAPTIVE_PORTAL_H
