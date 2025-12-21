#ifndef KEYBOARD_LAYOUT_H
#define KEYBOARD_LAYOUT_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// Keyboard layout identifiers
typedef enum {
    LAYOUT_US = 0,        // US English (QWERTY)
    LAYOUT_CH_DE,         // Swiss German
    LAYOUT_DE,            // German (QWERTZ)
    LAYOUT_FR,            // French (AZERTY)
    LAYOUT_UK,            // UK English
    LAYOUT_ES,            // Spanish
    LAYOUT_IT,            // Italian
    LAYOUT_COUNT          // Number of layouts
} keyboard_layout_t;

// Layout info structure
typedef struct {
    keyboard_layout_t id;
    const char *code;     // Short code (e.g., "ch-de")
    const char *name;     // Display name (e.g., "Swiss German")
} keyboard_layout_info_t;

/**
 * Initialize keyboard layout module and load saved layout from NVS
 */
esp_err_t keyboard_layout_init(void);

/**
 * Get the current keyboard layout
 */
keyboard_layout_t keyboard_layout_get(void);

/**
 * Set the keyboard layout and save to NVS
 */
esp_err_t keyboard_layout_set(keyboard_layout_t layout);

/**
 * Set keyboard layout by code string (e.g., "ch-de")
 */
esp_err_t keyboard_layout_set_by_code(const char *code);

/**
 * Get layout info for a specific layout
 */
const keyboard_layout_info_t *keyboard_layout_get_info(keyboard_layout_t layout);

/**
 * Get all available layouts
 */
const keyboard_layout_info_t *keyboard_layout_get_all(int *count);

/**
 * Convert a Unicode codepoint to HID keycode + modifiers for current layout
 * Returns keycode in lower byte, modifiers in upper byte
 * Returns 0 if character is not supported
 */
uint16_t keyboard_layout_char_to_keycode(uint32_t codepoint);

/**
 * Convert a UTF-8 string to keycodes, calling callback for each
 * Returns number of characters processed
 */
typedef void (*keycode_callback_t)(uint8_t keycode, uint8_t modifiers, void *ctx);
int keyboard_layout_string_to_keycodes(const char *utf8_str, keycode_callback_t callback, void *ctx);

#endif // KEYBOARD_LAYOUT_H
