#include "ota_handler.h"
#include "config.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_app_format.h"

static const char *TAG = "ota";

static ota_progress_t s_progress = {0};
static bool s_busy = false;
static char s_ota_url[256] = {0};

// OTA task handle
static TaskHandle_t s_ota_task = NULL;

// OTA update task
static void ota_task(void *pvParameter)
{
    const char *url = (const char *)pvParameter;
    esp_err_t err;

    ESP_LOGI(TAG, "Starting OTA from: %s", url);

    s_progress.status = OTA_STATUS_DOWNLOADING;
    s_progress.progress = 0;
    s_progress.downloaded = 0;
    s_progress.total_size = 0;
    s_progress.error_msg[0] = '\0';

    esp_http_client_config_t http_config = {
        .url = url,
        .timeout_ms = CONFIG_OTA_RECV_TIMEOUT_MS,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    esp_https_ota_handle_t https_ota_handle = NULL;

    err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(err));
        snprintf(s_progress.error_msg, sizeof(s_progress.error_msg),
                 "Begin failed: %s", esp_err_to_name(err));
        s_progress.status = OTA_STATUS_FAILED;
        s_busy = false;
        vTaskDelete(NULL);
        return;
    }

    // Get image size
    s_progress.total_size = esp_https_ota_get_image_size(https_ota_handle);
    ESP_LOGI(TAG, "Firmware size: %d bytes", s_progress.total_size);

    // Download and flash
    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }

        s_progress.downloaded = esp_https_ota_get_image_len_read(https_ota_handle);
        if (s_progress.total_size > 0) {
            s_progress.progress = (s_progress.downloaded * 100) / s_progress.total_size;
        }

        ESP_LOGD(TAG, "Downloaded: %d / %d (%d%%)",
                 s_progress.downloaded, s_progress.total_size, s_progress.progress);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA perform failed: %s", esp_err_to_name(err));
        snprintf(s_progress.error_msg, sizeof(s_progress.error_msg),
                 "Download failed: %s", esp_err_to_name(err));
        s_progress.status = OTA_STATUS_FAILED;
        esp_https_ota_abort(https_ota_handle);
        s_busy = false;
        vTaskDelete(NULL);
        return;
    }

    s_progress.status = OTA_STATUS_VERIFYING;
    s_progress.progress = 100;

    // Verify and finish
    if (!esp_https_ota_is_complete_data_received(https_ota_handle)) {
        ESP_LOGE(TAG, "Complete data was not received");
        snprintf(s_progress.error_msg, sizeof(s_progress.error_msg),
                 "Incomplete download");
        s_progress.status = OTA_STATUS_FAILED;
        esp_https_ota_abort(https_ota_handle);
        s_busy = false;
        vTaskDelete(NULL);
        return;
    }

    err = esp_https_ota_finish(https_ota_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed");
            snprintf(s_progress.error_msg, sizeof(s_progress.error_msg),
                     "Validation failed");
        } else {
            ESP_LOGE(TAG, "OTA finish failed: %s", esp_err_to_name(err));
            snprintf(s_progress.error_msg, sizeof(s_progress.error_msg),
                     "Finish failed: %s", esp_err_to_name(err));
        }
        s_progress.status = OTA_STATUS_FAILED;
        s_busy = false;
        vTaskDelete(NULL);
        return;
    }

    s_progress.status = OTA_STATUS_SUCCESS;
    s_busy = false;

    ESP_LOGI(TAG, "OTA update successful! Restarting in 2 seconds...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    vTaskDelete(NULL);
}

esp_err_t ota_handler_init(void)
{
    // Mark current partition as valid on successful boot
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;

    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG, "First boot after OTA, marking as valid");
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }

    ESP_LOGI(TAG, "Running partition: %s", running->label);
    ESP_LOGI(TAG, "Firmware version: %s", CONFIG_APP_VERSION);

    return ESP_OK;
}

esp_err_t ota_handler_start(const char *url)
{
    if (s_busy) {
        ESP_LOGW(TAG, "OTA already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    if (url == NULL || strlen(url) == 0) {
        ESP_LOGE(TAG, "Invalid URL");
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(s_ota_url, url, sizeof(s_ota_url) - 1);
    s_busy = true;

    // Create OTA task
    BaseType_t ret = xTaskCreate(ota_task, "ota_task", 8192, s_ota_url, 5, &s_ota_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA task");
        s_busy = false;
        return ESP_FAIL;
    }

    return ESP_OK;
}

ota_progress_t ota_handler_get_progress(void)
{
    return s_progress;
}

bool ota_handler_is_busy(void)
{
    return s_busy;
}

const char *ota_handler_get_version(void)
{
    return CONFIG_APP_VERSION;
}

esp_err_t ota_handler_mark_valid(void)
{
    return esp_ota_mark_app_valid_cancel_rollback();
}

esp_err_t ota_handler_rollback(void)
{
    esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Rollback failed: %s", esp_err_to_name(err));
    }
    return err;
}
