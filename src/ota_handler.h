#ifndef OTA_HANDLER_H
#define OTA_HANDLER_H

#include "esp_err.h"
#include <stdbool.h>

// OTA status
typedef enum {
    OTA_STATUS_IDLE,
    OTA_STATUS_DOWNLOADING,
    OTA_STATUS_VERIFYING,
    OTA_STATUS_SUCCESS,
    OTA_STATUS_FAILED,
} ota_status_t;

// OTA progress info
typedef struct {
    ota_status_t status;
    int progress;           // 0-100 percentage
    int total_size;         // Total firmware size
    int downloaded;         // Bytes downloaded
    char error_msg[64];     // Error message if failed
} ota_progress_t;

/**
 * Initialize OTA handler
 */
esp_err_t ota_handler_init(void);

/**
 * Start OTA update from URL
 * This runs in a separate task and updates progress
 */
esp_err_t ota_handler_start(const char *url);

/**
 * Get current OTA progress
 */
ota_progress_t ota_handler_get_progress(void);

/**
 * Check if OTA is in progress
 */
bool ota_handler_is_busy(void);

/**
 * Get the currently running firmware version
 */
const char *ota_handler_get_version(void);

/**
 * Mark current firmware as valid (after successful boot)
 */
esp_err_t ota_handler_mark_valid(void);

/**
 * Rollback to previous firmware
 */
esp_err_t ota_handler_rollback(void);

#endif // OTA_HANDLER_H
