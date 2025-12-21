#include "wifi_manager.h"
#include "config.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_netif.h"

static const char *TAG = "wifi_mgr";

// Event group bits
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// Static variables
static EventGroupHandle_t s_wifi_event_group;
static esp_netif_t *s_sta_netif = NULL;
static esp_netif_t *s_ap_netif = NULL;
static wifi_manager_status_t s_status = {0};
static int s_retry_count = 0;

// Forward declarations
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);
static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data);

esp_err_t wifi_manager_init(void)
{
    esp_err_t ret;

    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Create event group
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_FAIL;
    }

    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default network interfaces
    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();

    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &ip_event_handler,
                                                        NULL,
                                                        NULL));

    ESP_LOGI(TAG, "WiFi manager initialized");
    return ESP_OK;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "STA started, connecting...");
                esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                s_status.connected = false;
                if (s_retry_count < CONFIG_WIFI_MAX_RETRY) {
                    esp_wifi_connect();
                    s_retry_count++;
                    ESP_LOGI(TAG, "Retry connecting to AP (%d/%d)",
                             s_retry_count, CONFIG_WIFI_MAX_RETRY);
                } else {
                    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                    ESP_LOGE(TAG, "Failed to connect to AP");
                }
                break;

            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "AP started: %s", CONFIG_AP_SSID);
                s_status.mode = WIFI_MODE_AP;
                break;

            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
                ESP_LOGI(TAG, "Station connected, MAC: " MACSTR, MAC2STR(event->mac));
                break;
            }

            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
                ESP_LOGI(TAG, "Station disconnected, MAC: " MACSTR, MAC2STR(event->mac));
                break;
            }

            default:
                break;
        }
    }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(s_status.ip_addr, sizeof(s_status.ip_addr), IPSTR,
                 IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Got IP: %s", s_status.ip_addr);
        s_status.connected = true;
        s_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_manager_start(void)
{
    if (wifi_manager_has_credentials()) {
        ESP_LOGI(TAG, "Credentials found, starting STA mode");
        return wifi_manager_start_sta();
    } else {
        ESP_LOGI(TAG, "No credentials, starting AP mode");
        return wifi_manager_start_ap();
    }
}

esp_err_t wifi_manager_start_ap(void)
{
    ESP_ERROR_CHECK(esp_wifi_stop());

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = CONFIG_AP_SSID,
            .ssid_len = strlen(CONFIG_AP_SSID),
            .channel = 1,
            .password = "",  // Open network
            .max_connection = CONFIG_AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_OPEN,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_status.mode = WIFI_MODE_AP;
    s_status.connected = false;
    strncpy(s_status.ssid, CONFIG_AP_SSID, sizeof(s_status.ssid) - 1);
    strncpy(s_status.ip_addr, CONFIG_AP_IP, sizeof(s_status.ip_addr) - 1);

    ESP_LOGI(TAG, "AP mode started. SSID: %s", CONFIG_AP_SSID);
    return ESP_OK;
}

esp_err_t wifi_manager_start_sta(void)
{
    esp_err_t ret;
    nvs_handle_t nvs;
    char ssid[33] = {0};
    char password[65] = {0};
    size_t ssid_len = sizeof(ssid);
    size_t pass_len = sizeof(password);

    // Read credentials from NVS
    ret = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS");
        return ret;
    }

    ret = nvs_get_str(nvs, CONFIG_NVS_KEY_SSID, ssid, &ssid_len);
    if (ret != ESP_OK) {
        nvs_close(nvs);
        ESP_LOGE(TAG, "Failed to read SSID from NVS");
        return ret;
    }

    ret = nvs_get_str(nvs, CONFIG_NVS_KEY_PASS, password, &pass_len);
    if (ret != ESP_OK) {
        nvs_close(nvs);
        ESP_LOGE(TAG, "Failed to read password from NVS");
        return ret;
    }
    nvs_close(nvs);

    ESP_ERROR_CHECK(esp_wifi_stop());

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_status.mode = WIFI_MODE_STA;
    strncpy(s_status.ssid, ssid, sizeof(s_status.ssid) - 1);

    ESP_LOGI(TAG, "STA mode started, connecting to: %s", ssid);

    // Wait for connection
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdTRUE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(CONFIG_WIFI_CONNECT_TIMEOUT_S * 1000));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to %s", ssid);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to %s", ssid);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "Connection timeout");
        return ESP_ERR_TIMEOUT;
    }
}

bool wifi_manager_has_credentials(void)
{
    nvs_handle_t nvs;
    esp_err_t ret;

    ret = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (ret != ESP_OK) {
        return false;
    }

    size_t ssid_len = 0;
    ret = nvs_get_str(nvs, CONFIG_NVS_KEY_SSID, NULL, &ssid_len);
    nvs_close(nvs);

    return (ret == ESP_OK && ssid_len > 1);
}

esp_err_t wifi_manager_save_credentials(const char *ssid, const char *password)
{
    nvs_handle_t nvs;
    esp_err_t ret;

    if (ssid == NULL || strlen(ssid) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing");
        return ret;
    }

    ret = nvs_set_str(nvs, CONFIG_NVS_KEY_SSID, ssid);
    if (ret != ESP_OK) {
        nvs_close(nvs);
        ESP_LOGE(TAG, "Failed to save SSID");
        return ret;
    }

    ret = nvs_set_str(nvs, CONFIG_NVS_KEY_PASS, password ? password : "");
    if (ret != ESP_OK) {
        nvs_close(nvs);
        ESP_LOGE(TAG, "Failed to save password");
        return ret;
    }

    ret = nvs_commit(nvs);
    nvs_close(nvs);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Credentials saved for SSID: %s", ssid);
    }

    return ret;
}

esp_err_t wifi_manager_clear_credentials(void)
{
    nvs_handle_t nvs;
    esp_err_t ret;

    ret = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (ret != ESP_OK) {
        return ret;
    }

    nvs_erase_key(nvs, CONFIG_NVS_KEY_SSID);
    nvs_erase_key(nvs, CONFIG_NVS_KEY_PASS);
    ret = nvs_commit(nvs);
    nvs_close(nvs);

    ESP_LOGI(TAG, "Credentials cleared");
    return ret;
}

wifi_manager_status_t wifi_manager_get_status(void)
{
    // Update RSSI if connected
    if (s_status.mode == WIFI_MODE_STA && s_status.connected) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            s_status.rssi = ap_info.rssi;
        }
    }
    return s_status;
}

int wifi_manager_scan(char results[][33], int8_t rssi[], int max_results)
{
    uint16_t ap_count = 0;
    wifi_ap_record_t *ap_list = NULL;

    // Start scan
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
    };

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));

    if (ap_count == 0) {
        return 0;
    }

    ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (ap_list == NULL) {
        return 0;
    }

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_list));

    int count = (ap_count < max_results) ? ap_count : max_results;
    for (int i = 0; i < count; i++) {
        strncpy(results[i], (char *)ap_list[i].ssid, 32);
        results[i][32] = '\0';
        if (rssi != NULL) {
            rssi[i] = ap_list[i].rssi;
        }
    }

    free(ap_list);
    ESP_LOGI(TAG, "Scan complete, found %d networks", count);
    return count;
}

esp_err_t wifi_manager_try_connect(const char *ssid, const char *password)
{
    esp_err_t ret;

    ESP_ERROR_CHECK(esp_wifi_stop());

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    s_retry_count = 0;
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_status.mode = WIFI_MODE_STA;
    strncpy(s_status.ssid, ssid, sizeof(s_status.ssid) - 1);

    // Wait for connection
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdTRUE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(CONFIG_WIFI_CONNECT_TIMEOUT_S * 1000));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Test connection successful to %s", ssid);
        ret = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Test connection failed to %s", ssid);
        ret = ESP_FAIL;
    }

    return ret;
}
