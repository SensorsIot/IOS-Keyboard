#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "mdns.h"

#include "config.h"
#include "wifi_manager.h"
#include "captive_portal.h"
#include "debug_server.h"
#include "ota_handler.h"
#include "keyboard_layout.h"
#if CONFIG_ENABLE_HID
#include "usb_hid.h"
#endif
#if CONFIG_ENABLE_BLE
#include "ble_gatt.h"
#include "command_parser.h"
#endif

static const char *TAG = "main";

static void start_mdns(void)
{
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS init failed: %s", esp_err_to_name(err));
        return;
    }

    mdns_hostname_set(CONFIG_MDNS_HOSTNAME);
    mdns_instance_name_set("IOS Keyboard");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);

    ESP_LOGI(TAG, "mDNS started: http://%s.local", CONFIG_MDNS_HOSTNAME);
}

void app_main(void)
{
    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "  IOS-Keyboard v%s", CONFIG_APP_VERSION);
#if CONFIG_ENABLE_HID && CONFIG_ENABLE_BLE
    ESP_LOGI(TAG, "  Phase 2: HID + BLE");
#elif CONFIG_ENABLE_HID
    ESP_LOGI(TAG, "  Phase 2: HID Keyboard");
#else
    ESP_LOGI(TAG, "  Phase 1: OTA Testing");
#endif
    ESP_LOGI(TAG, "=================================");

    // Initialize OTA handler
    ESP_ERROR_CHECK(ota_handler_init());

    // Initialize keyboard layout (loads from NVS)
    ESP_ERROR_CHECK(keyboard_layout_init());
    const keyboard_layout_info_t *layout = keyboard_layout_get_info(keyboard_layout_get());
    ESP_LOGI(TAG, "Keyboard layout: %s", layout ? layout->name : "Unknown");

    // Initialize WiFi manager
    ESP_ERROR_CHECK(wifi_manager_init());

    // Start WiFi (AP or STA based on stored credentials)
    wifi_manager_start();

    wifi_manager_status_t status = wifi_manager_get_status();

    if (status.mode == WIFI_MGR_MODE_AP) {
        ESP_LOGI(TAG, "Starting in AP mode for configuration");
        ESP_LOGI(TAG, "Connect to WiFi: %s", CONFIG_AP_SSID);
        ESP_LOGI(TAG, "Open browser to: http://%s", CONFIG_AP_IP);

        start_mdns();
        captive_portal_start();
    } else if (status.mode == WIFI_MGR_MODE_STA) {
        if (status.connected) {
            start_mdns();

            ESP_LOGI(TAG, "Connected to WiFi: %s", status.ssid);
            ESP_LOGI(TAG, "IP Address: %s", status.ip_addr);
            ESP_LOGI(TAG, "Access via: http://%s.local or http://%s", 
                     CONFIG_MDNS_HOSTNAME, status.ip_addr);

            debug_server_start();
            debug_server_log("Device started, connected to %s", status.ssid);

#if CONFIG_ENABLE_HID
            // Initialize USB HID after WiFi is stable (so portal works for recovery)
            ESP_LOGI(TAG, "Initializing USB HID...");
            esp_err_t hid_err = usb_hid_init();
            if (hid_err != ESP_OK) {
                ESP_LOGE(TAG, "HID init failed: %s", esp_err_to_name(hid_err));
                debug_server_log("HID init failed: %s", esp_err_to_name(hid_err));
            } else {
                ESP_LOGI(TAG, "HID keyboard enabled");
                debug_server_log("HID keyboard enabled");
            }

#else
            ESP_LOGI(TAG, "HID disabled (Phase 1 - OTA testing)");
            debug_server_log("HID disabled - Phase 1 OTA testing mode");
#endif

#if CONFIG_ENABLE_BLE
            // Initialize BLE last (WiFi/debug server must work first for debugging)
            ESP_LOGI(TAG, "Initializing BLE GATT...");
            debug_server_log("Starting BLE...");
            esp_err_t ble_err = ble_gatt_init();
            if (ble_err == ESP_OK) {
                command_parser_init();
                ble_gatt_set_rx_callback(command_parser_process);
                ble_err = ble_gatt_start();
            }
            if (ble_err == ESP_OK) {
                ESP_LOGI(TAG, "BLE advertising as '%s'", CONFIG_BLE_DEVICE_NAME);
                debug_server_log("BLE enabled - connect from iPhone");
            } else {
                ESP_LOGE(TAG, "BLE init failed: %s", esp_err_to_name(ble_err));
                debug_server_log("BLE failed: %s", esp_err_to_name(ble_err));
            }
#endif
        } else {
            ESP_LOGW(TAG, "Failed to connect, starting AP mode");
            wifi_manager_start_ap();
            start_mdns();
            captive_portal_start();
        }
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
