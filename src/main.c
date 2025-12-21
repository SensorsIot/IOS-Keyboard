#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"

#include "config.h"
#include "wifi_manager.h"
#include "captive_portal.h"
#include "debug_server.h"
#include "ota_handler.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "  IOS-Keyboard v%s", CONFIG_APP_VERSION);
    ESP_LOGI(TAG, "  Phase 1: OTA Testing");
    ESP_LOGI(TAG, "=================================");

    // Initialize OTA handler (marks firmware as valid if needed)
    ESP_ERROR_CHECK(ota_handler_init());

    // Initialize WiFi manager
    ESP_ERROR_CHECK(wifi_manager_init());

    // Start WiFi (AP or STA based on stored credentials)
    esp_err_t ret = wifi_manager_start();

    wifi_manager_status_t status = wifi_manager_get_status();

    if (status.mode == WIFI_MODE_AP) {
        // No credentials - start captive portal
        ESP_LOGI(TAG, "Starting in AP mode for configuration");
        ESP_LOGI(TAG, "Connect to WiFi: %s", CONFIG_AP_SSID);
        ESP_LOGI(TAG, "Open browser to: http://%s", CONFIG_AP_IP);

        captive_portal_start();
    } else if (status.mode == WIFI_MODE_STA) {
        if (status.connected) {
            // Connected to WiFi - start debug server
            ESP_LOGI(TAG, "Connected to WiFi: %s", status.ssid);
            ESP_LOGI(TAG, "IP Address: %s", status.ip_addr);
            ESP_LOGI(TAG, "Debug interface: http://%s", status.ip_addr);

            debug_server_start();
            debug_server_log("Device started, connected to %s", status.ssid);

#if CONFIG_ENABLE_HID
            ESP_LOGI(TAG, "HID keyboard enabled");
            // TODO: Initialize USB HID and type "hello world"
#else
            ESP_LOGI(TAG, "HID disabled (Phase 1 - OTA testing)");
            debug_server_log("HID disabled - Phase 1 OTA testing mode");
#endif
        } else {
            // Failed to connect - fall back to AP mode
            ESP_LOGW(TAG, "Failed to connect to stored network, starting AP mode");
            wifi_manager_start_ap();
            captive_portal_start();
        }
    }

    // Main loop - just keep running
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));

        // Periodic status log
        status = wifi_manager_get_status();
        if (status.mode == WIFI_MODE_STA && status.connected) {
            ESP_LOGD(TAG, "Status: Connected to %s, RSSI: %d dBm, Free heap: %lu",
                     status.ssid, status.rssi, esp_get_free_heap_size());
        }
    }
}
