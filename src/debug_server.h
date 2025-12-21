#ifndef DEBUG_SERVER_H
#define DEBUG_SERVER_H

#include "esp_err.h"

/**
 * Start the debug web server
 * Serves debug dashboard when connected to WiFi in STA mode
 */
esp_err_t debug_server_start(void);

/**
 * Stop the debug web server
 */
esp_err_t debug_server_stop(void);

/**
 * Check if debug server is running
 */
bool debug_server_is_running(void);

/**
 * Add a log message to the buffer
 */
void debug_server_log(const char *format, ...);

#endif // DEBUG_SERVER_H
