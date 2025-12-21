#include "usb_hid.h"
#include "config.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "tinyusb.h"
#include "class/hid/hid_device.h"

static const char *TAG = "usb_hid";

// HID Report Descriptor for keyboard only
static const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(1))
};

// String descriptors
static const char *hid_string_descriptor[] = {
    (char[]){0x09, 0x04},  // 0: Supported language (English)
    "IOS-Keyboard",        // 1: Manufacturer
    "USB Keyboard",        // 2: Product
    "000001",              // 3: Serial
    "HID Keyboard",        // 4: HID interface
};

// Configuration descriptor length
#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

// Configuration descriptor
static const uint8_t hid_configuration_descriptor[] = {
    // Config: config number, interface count, string index, total length, attributes, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    // HID: interface number, string index, boot protocol, report descriptor len, EP In address, size, polling interval
    TUD_HID_DESCRIPTOR(0, 4, HID_ITF_PROTOCOL_KEYBOARD, sizeof(hid_report_descriptor), 0x81, 16, 10),
};

// USB device ready flag
static bool s_usb_ready = false;

// ASCII to HID keycode mapping (US layout)
// Returns keycode in lower byte, shift modifier in upper byte
static uint16_t ascii_to_keycode(char c)
{
    // Letters a-z
    if (c >= 'a' && c <= 'z') {
        return HID_KEY_A + (c - 'a');
    }
    // Letters A-Z (with shift)
    if (c >= 'A' && c <= 'Z') {
        return (HID_KEY_A + (c - 'A')) | (KEYBOARD_MODIFIER_LEFTSHIFT << 8);
    }
    // Numbers 1-9
    if (c >= '1' && c <= '9') {
        return HID_KEY_1 + (c - '1');
    }
    // Number 0
    if (c == '0') {
        return HID_KEY_0;
    }
    // Special characters
    switch (c) {
        case ' ':  return HID_KEY_SPACE;
        case '\n': return HID_KEY_ENTER;
        case '\t': return HID_KEY_TAB;
        case '.':  return HID_KEY_PERIOD;
        case ',':  return HID_KEY_COMMA;
        case '!':  return HID_KEY_1 | (KEYBOARD_MODIFIER_LEFTSHIFT << 8);
        case '@':  return HID_KEY_2 | (KEYBOARD_MODIFIER_LEFTSHIFT << 8);
        case '#':  return HID_KEY_3 | (KEYBOARD_MODIFIER_LEFTSHIFT << 8);
        case '$':  return HID_KEY_4 | (KEYBOARD_MODIFIER_LEFTSHIFT << 8);
        case '%':  return HID_KEY_5 | (KEYBOARD_MODIFIER_LEFTSHIFT << 8);
        case '^':  return HID_KEY_6 | (KEYBOARD_MODIFIER_LEFTSHIFT << 8);
        case '&':  return HID_KEY_7 | (KEYBOARD_MODIFIER_LEFTSHIFT << 8);
        case '*':  return HID_KEY_8 | (KEYBOARD_MODIFIER_LEFTSHIFT << 8);
        case '(':  return HID_KEY_9 | (KEYBOARD_MODIFIER_LEFTSHIFT << 8);
        case ')':  return HID_KEY_0 | (KEYBOARD_MODIFIER_LEFTSHIFT << 8);
        case '-':  return HID_KEY_MINUS;
        case '_':  return HID_KEY_MINUS | (KEYBOARD_MODIFIER_LEFTSHIFT << 8);
        case '=':  return HID_KEY_EQUAL;
        case '+':  return HID_KEY_EQUAL | (KEYBOARD_MODIFIER_LEFTSHIFT << 8);
        case '[':  return HID_KEY_BRACKET_LEFT;
        case ']':  return HID_KEY_BRACKET_RIGHT;
        case '{':  return HID_KEY_BRACKET_LEFT | (KEYBOARD_MODIFIER_LEFTSHIFT << 8);
        case '}':  return HID_KEY_BRACKET_RIGHT | (KEYBOARD_MODIFIER_LEFTSHIFT << 8);
        case '\\': return HID_KEY_BACKSLASH;
        case '|':  return HID_KEY_BACKSLASH | (KEYBOARD_MODIFIER_LEFTSHIFT << 8);
        case ';':  return HID_KEY_SEMICOLON;
        case ':':  return HID_KEY_SEMICOLON | (KEYBOARD_MODIFIER_LEFTSHIFT << 8);
        case '\'': return HID_KEY_APOSTROPHE;
        case '"':  return HID_KEY_APOSTROPHE | (KEYBOARD_MODIFIER_LEFTSHIFT << 8);
        case '/':  return HID_KEY_SLASH;
        case '?':  return HID_KEY_SLASH | (KEYBOARD_MODIFIER_LEFTSHIFT << 8);
        case '<':  return HID_KEY_COMMA | (KEYBOARD_MODIFIER_LEFTSHIFT << 8);
        case '>':  return HID_KEY_PERIOD | (KEYBOARD_MODIFIER_LEFTSHIFT << 8);
        case '`':  return HID_KEY_GRAVE;
        case '~':  return HID_KEY_GRAVE | (KEYBOARD_MODIFIER_LEFTSHIFT << 8);
        default:   return 0;
    }
}

// Report ID for keyboard (must match HID_REPORT_ID in descriptor)
#define KEYBOARD_REPORT_ID 1

// Send a single key press and release
static esp_err_t send_key(uint8_t keycode, uint8_t modifier)
{
    uint8_t keycodes[6] = {0};

    // Key press
    keycodes[0] = keycode;
    if (!tud_hid_keyboard_report(KEYBOARD_REPORT_ID, modifier, keycodes)) {
        return ESP_FAIL;
    }
    vTaskDelay(pdMS_TO_TICKS(CONFIG_TYPING_DELAY_MS));

    // Key release
    memset(keycodes, 0, sizeof(keycodes));
    if (!tud_hid_keyboard_report(KEYBOARD_REPORT_ID, 0, keycodes)) {
        return ESP_FAIL;
    }
    vTaskDelay(pdMS_TO_TICKS(CONFIG_TYPING_DELAY_MS / 2));

    return ESP_OK;
}

// TinyUSB callbacks
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

void tud_mount_cb(void)
{
    ESP_LOGI(TAG, "USB mounted");
    s_usb_ready = true;
}

void tud_umount_cb(void)
{
    ESP_LOGI(TAG, "USB unmounted");
    s_usb_ready = false;
}

esp_err_t usb_hid_init(void)
{
    ESP_LOGI(TAG, "Initializing USB HID keyboard");

    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,  // Use default from Kconfig
        .string_descriptor = hid_string_descriptor,
        .string_descriptor_count = sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]),
        .external_phy = false,
        .configuration_descriptor = hid_configuration_descriptor,
        .self_powered = false,
        .vbus_monitor_io = -1,
    };

    esp_err_t ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TinyUSB driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "USB HID keyboard initialized");
    return ESP_OK;
}

esp_err_t usb_hid_type_text(const char *text)
{
    if (!s_usb_ready) {
        ESP_LOGW(TAG, "USB not ready");
        return ESP_ERR_INVALID_STATE;
    }

    if (text == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Typing: %s", text);

    for (size_t i = 0; i < strlen(text); i++) {
        uint16_t keydata = ascii_to_keycode(text[i]);
        if (keydata == 0) {
            ESP_LOGW(TAG, "Unsupported character: 0x%02x", text[i]);
            continue;
        }

        uint8_t keycode = keydata & 0xFF;
        uint8_t modifier = (keydata >> 8) & 0xFF;

        esp_err_t ret = send_key(keycode, modifier);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send key");
            return ret;
        }
    }

    return ESP_OK;
}

esp_err_t usb_hid_type_hello_world(void)
{
    return usb_hid_type_text("hello world");
}

bool usb_hid_is_ready(void)
{
    return s_usb_ready;
}

esp_err_t usb_hid_send_backspace(void)
{
    if (!s_usb_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    return send_key(HID_KEY_BACKSPACE, 0);
}

esp_err_t usb_hid_send_enter(void)
{
    if (!s_usb_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    return send_key(HID_KEY_ENTER, 0);
}
