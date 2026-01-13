#ifndef USB_HID_H
#define USB_HID_H

#include <stdbool.h>
#include "esp_err.h"

/**
 * Initialize USB HID keyboard
 */
esp_err_t usb_hid_init(void);

/**
 * Type a string as keyboard input
 * @param text String to type
 * @return ESP_OK on success
 */
esp_err_t usb_hid_type_text(const char *text);

/**
 * Type "hello world" test message
 */
esp_err_t usb_hid_type_hello_world(void);

/**
 * Check if USB HID is ready
 */
bool usb_hid_is_ready(void);

/**
 * Send a single backspace keystroke
 */
esp_err_t usb_hid_send_backspace(void);

/**
 * Send a single enter keystroke
 */
esp_err_t usb_hid_send_enter(void);

/**
 * Send Ctrl+key combo
 * @param key ASCII character (e.g., 'J' for Ctrl+J)
 */
esp_err_t usb_hid_send_ctrl_key(char key);

#endif // USB_HID_H
