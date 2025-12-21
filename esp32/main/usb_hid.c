#include "usb_hid.h"
#include "config.h"
#include "keyboard_layout.h"
#include "debug_server.h"

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

// HID country codes (USB HID spec)
#define HID_COUNTRY_US          33  // 0x21
#define HID_COUNTRY_SWISS_DE    28  // 0x1C
#define HID_COUNTRY_GERMAN       9  // 0x09
#define HID_COUNTRY_FRENCH       8  // 0x08
#define HID_COUNTRY_UK          32  // 0x20
#define HID_COUNTRY_SPANISH     25  // 0x19
#define HID_COUNTRY_ITALIAN     14  // 0x0E

// Offset of country code byte in configuration descriptor
// Config(9) + Interface(9) + HID header(4) = byte 22
#define HID_COUNTRY_CODE_OFFSET 22

// Configuration descriptor (mutable for country code)
static uint8_t hid_configuration_descriptor[] = {
    // Config: config number, interface count, string index, total length, attributes, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    // HID: interface number, string index, boot protocol, report descriptor len, EP In address, size, polling interval
    TUD_HID_DESCRIPTOR(0, 4, HID_ITF_PROTOCOL_KEYBOARD, sizeof(hid_report_descriptor), 0x81, 16, 10),
};

// Get HID country code for current keyboard layout
static uint8_t get_hid_country_code(void)
{
    keyboard_layout_t layout = keyboard_layout_get();
    switch (layout) {
        case LAYOUT_US:    return HID_COUNTRY_US;
        case LAYOUT_CH_DE: return HID_COUNTRY_SWISS_DE;
        case LAYOUT_DE:    return HID_COUNTRY_GERMAN;
        case LAYOUT_FR:    return HID_COUNTRY_FRENCH;
        case LAYOUT_UK:    return HID_COUNTRY_UK;
        case LAYOUT_ES:    return HID_COUNTRY_SPANISH;
        case LAYOUT_IT:    return HID_COUNTRY_ITALIAN;
        default:           return HID_COUNTRY_US;
    }
}

// USB device ready flag
static bool s_usb_ready = false;

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

    // Set HID country code based on current keyboard layout
    uint8_t country_code = get_hid_country_code();
    hid_configuration_descriptor[HID_COUNTRY_CODE_OFFSET] = country_code;
    ESP_LOGI(TAG, "HID country code: %d", country_code);

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

// Context for typing callback
typedef struct {
    esp_err_t result;
    const char *text;  // Current text being typed
    int index;         // Current character index
} type_context_t;

// Callback for keyboard_layout_string_to_keycodes
static void type_key_callback(uint8_t keycode, uint8_t modifiers, void *ctx)
{
    type_context_t *type_ctx = (type_context_t *)ctx;
    if (type_ctx->result != ESP_OK) {
        return;  // Stop on first error
    }

    // Get the character being typed for trace
    char ch = type_ctx->text[type_ctx->index++];
    // Show printable char, or hex for non-printable
    if (ch >= 32 && ch < 127) {
        debug_server_trace_hid("'%c' K:0x%02X M:0x%02X", ch, keycode, modifiers);
    } else {
        debug_server_trace_hid("0x%02X K:0x%02X M:0x%02X", (uint8_t)ch, keycode, modifiers);
    }

    type_ctx->result = send_key(keycode, modifiers);
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

    type_context_t ctx = { .result = ESP_OK, .text = text, .index = 0 };
    int count = keyboard_layout_string_to_keycodes(text, type_key_callback, &ctx);

    ESP_LOGI(TAG, "Typed %d characters", count);
    return ctx.result;
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
    debug_server_trace_hid("BS K:0x%02X", HID_KEY_BACKSPACE);
    return send_key(HID_KEY_BACKSPACE, 0);
}

esp_err_t usb_hid_send_enter(void)
{
    if (!s_usb_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    debug_server_trace_hid("ENTER K:0x%02X", HID_KEY_ENTER);
    return send_key(HID_KEY_ENTER, 0);
}

esp_err_t usb_hid_send_ctrl_key(char key)
{
    if (!s_usb_ready) {
        return ESP_ERR_INVALID_STATE;
    }

    // Convert ASCII letter to HID keycode
    uint8_t keycode = 0;
    if (key >= 'A' && key <= 'Z') {
        keycode = HID_KEY_A + (key - 'A');
    } else if (key >= 'a' && key <= 'z') {
        keycode = HID_KEY_A + (key - 'a');
    } else {
        ESP_LOGW(TAG, "Unsupported Ctrl+key: %c", key);
        return ESP_ERR_INVALID_ARG;
    }

    debug_server_trace_hid("CTRL+%c K:0x%02X M:0x02", key, keycode);
    // Send with Left Ctrl modifier
    return send_key(keycode, KEYBOARD_MODIFIER_LEFTCTRL);
}
