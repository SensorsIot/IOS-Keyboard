#include "command_parser.h"
#include "config.h"
#include "usb_hid.h"

#include <string.h>
#include "esp_log.h"

static const char *TAG = "cmd_parser";

esp_err_t command_parser_init(void)
{
    ESP_LOGI(TAG, "Command parser initialized");
    return ESP_OK;
}

void command_parser_process(const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0) {
        ESP_LOGW(TAG, "Empty command received");
        return;
    }

    uint8_t cmd = data[0];

    switch (cmd) {
        case CMD_BACKSPACE: {
            // 0x01 <count> - send backspace count times
            if (len < 2) {
                ESP_LOGW(TAG, "Backspace command missing count");
                return;
            }
            uint8_t count = data[1];
            ESP_LOGI(TAG, "Backspace x%d", count);
            for (uint8_t i = 0; i < count; i++) {
                esp_err_t ret = usb_hid_send_backspace();
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to send backspace: %s", esp_err_to_name(ret));
                    return;
                }
            }
            break;
        }

        case CMD_INSERT: {
            // 0x02 <text> - type the text
            if (len < 2) {
                ESP_LOGW(TAG, "Insert command missing text");
                return;
            }
            // Create null-terminated string from payload
            size_t text_len = len - 1;
            char text[text_len + 1];
            memcpy(text, &data[1], text_len);
            text[text_len] = '\0';

            ESP_LOGI(TAG, "Insert: %s", text);
            esp_err_t ret = usb_hid_type_text(text);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to type text: %s", esp_err_to_name(ret));
            }
            break;
        }

        case CMD_ENTER: {
            // 0x03 - send enter key
            ESP_LOGI(TAG, "Enter");
            esp_err_t ret = usb_hid_send_enter();
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to send enter: %s", esp_err_to_name(ret));
            }
            break;
        }

        default:
            ESP_LOGW(TAG, "Unknown command: 0x%02x", cmd);
            break;
    }
}
