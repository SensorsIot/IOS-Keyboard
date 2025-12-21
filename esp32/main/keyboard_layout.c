#include "keyboard_layout.h"
#include "config.h"

#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "class/hid/hid.h"

static const char *TAG = "kbd_layout";

// NVS key for storing layout
#define NVS_KEY_LAYOUT "kbd_layout"

// Current layout
static keyboard_layout_t s_current_layout = LAYOUT_US;

// Modifier flags
#define MOD_NONE   0
#define MOD_SHIFT  KEYBOARD_MODIFIER_LEFTSHIFT
#define MOD_ALTGR  KEYBOARD_MODIFIER_RIGHTALT

// Keycode entry: keycode | (modifier << 8)
#define KC(k)       (k)
#define KC_S(k)     ((k) | (MOD_SHIFT << 8))
#define KC_A(k)     ((k) | (MOD_ALTGR << 8))
#define KC_SA(k)    ((k) | ((MOD_SHIFT | MOD_ALTGR) << 8))

// Layout info table
static const keyboard_layout_info_t s_layout_info[] = {
    { LAYOUT_US,    "us",    "US English" },
    { LAYOUT_CH_DE, "ch-de", "Swiss German" },
    { LAYOUT_DE,    "de",    "German" },
    { LAYOUT_FR,    "fr",    "French" },
    { LAYOUT_UK,    "uk",    "UK English" },
    { LAYOUT_ES,    "es",    "Spanish" },
    { LAYOUT_IT,    "it",    "Italian" },
};

// ============================================================================
// US English Layout (QWERTY) - Base reference
// ============================================================================
static uint16_t layout_us_lookup(uint32_t cp) {
    // Basic ASCII
    if (cp >= 'a' && cp <= 'z') return KC(HID_KEY_A + (cp - 'a'));
    if (cp >= 'A' && cp <= 'Z') return KC_S(HID_KEY_A + (cp - 'A'));
    if (cp >= '1' && cp <= '9') return KC(HID_KEY_1 + (cp - '1'));
    if (cp == '0') return KC(HID_KEY_0);

    switch (cp) {
        case ' ':  return KC(HID_KEY_SPACE);
        case '\n': return KC(HID_KEY_ENTER);
        case '\t': return KC(HID_KEY_TAB);
        case '!':  return KC_S(HID_KEY_1);
        case '@':  return KC_S(HID_KEY_2);
        case '#':  return KC_S(HID_KEY_3);
        case '$':  return KC_S(HID_KEY_4);
        case '%':  return KC_S(HID_KEY_5);
        case '^':  return KC_S(HID_KEY_6);
        case '&':  return KC_S(HID_KEY_7);
        case '*':  return KC_S(HID_KEY_8);
        case '(':  return KC_S(HID_KEY_9);
        case ')':  return KC_S(HID_KEY_0);
        case '-':  return KC(HID_KEY_MINUS);
        case '_':  return KC_S(HID_KEY_MINUS);
        case '=':  return KC(HID_KEY_EQUAL);
        case '+':  return KC_S(HID_KEY_EQUAL);
        case '[':  return KC(HID_KEY_BRACKET_LEFT);
        case ']':  return KC(HID_KEY_BRACKET_RIGHT);
        case '{':  return KC_S(HID_KEY_BRACKET_LEFT);
        case '}':  return KC_S(HID_KEY_BRACKET_RIGHT);
        case '\\': return KC(HID_KEY_BACKSLASH);
        case '|':  return KC_S(HID_KEY_BACKSLASH);
        case ';':  return KC(HID_KEY_SEMICOLON);
        case ':':  return KC_S(HID_KEY_SEMICOLON);
        case '\'': return KC(HID_KEY_APOSTROPHE);
        case '"':  return KC_S(HID_KEY_APOSTROPHE);
        case '`':  return KC(HID_KEY_GRAVE);
        case '~':  return KC_S(HID_KEY_GRAVE);
        case ',':  return KC(HID_KEY_COMMA);
        case '.':  return KC(HID_KEY_PERIOD);
        case '<':  return KC_S(HID_KEY_COMMA);
        case '>':  return KC_S(HID_KEY_PERIOD);
        case '/':  return KC(HID_KEY_SLASH);
        case '?':  return KC_S(HID_KEY_SLASH);
        default:   return 0;
    }
}

// ============================================================================
// Swiss German Layout (QWERTZ)
// Physical key positions differ from US layout
// ============================================================================
static uint16_t layout_ch_de_lookup(uint32_t cp) {
    // Letters - Z and Y are swapped in QWERTZ
    if (cp >= 'a' && cp <= 'z') {
        if (cp == 'y') return KC(HID_KEY_Z);
        if (cp == 'z') return KC(HID_KEY_Y);
        return KC(HID_KEY_A + (cp - 'a'));
    }
    if (cp >= 'A' && cp <= 'Z') {
        if (cp == 'Y') return KC_S(HID_KEY_Z);
        if (cp == 'Z') return KC_S(HID_KEY_Y);
        return KC_S(HID_KEY_A + (cp - 'A'));
    }

    // Numbers (unshifted are same)
    if (cp >= '1' && cp <= '9') return KC(HID_KEY_1 + (cp - '1'));
    if (cp == '0') return KC(HID_KEY_0);

    switch (cp) {
        case ' ':  return KC(HID_KEY_SPACE);
        case '\n': return KC(HID_KEY_ENTER);
        case '\t': return KC(HID_KEY_TAB);

        // Swiss German shifted numbers
        case '+':  return KC_S(HID_KEY_1);
        case '"':  return KC_S(HID_KEY_2);
        case '*':  return KC_S(HID_KEY_3);
        // case 'ç':  return KC_S(HID_KEY_4);  // handled in unicode
        case '%':  return KC_S(HID_KEY_5);
        case '&':  return KC_S(HID_KEY_6);
        case '/':  return KC_S(HID_KEY_7);
        case '(':  return KC_S(HID_KEY_8);
        case ')':  return KC_S(HID_KEY_9);
        case '=':  return KC_S(HID_KEY_0);

        // Special characters on Swiss German
        case '\'': return KC(HID_KEY_MINUS);           // ' key (unshifted -)
        case '?':  return KC_S(HID_KEY_MINUS);         // ? key (shifted -)
        case '^':  return KC(HID_KEY_EQUAL);           // ^ key (dead key, unshifted =)
        case '`':  return KC_S(HID_KEY_EQUAL);         // ` key (dead key, shifted =)

        // Umlauts - on Swiss German keyboard
        case 0xFC: return KC(HID_KEY_BRACKET_LEFT);    // ü
        case 0xDC: return KC_S(HID_KEY_BRACKET_LEFT);  // Ü
        case 0xE8: return KC(HID_KEY_BRACKET_RIGHT);   // è (on Swiss keyboard)
        case 0xC8: return KC_S(HID_KEY_BRACKET_RIGHT); // È

        case 0xF6: return KC(HID_KEY_SEMICOLON);       // ö
        case 0xD6: return KC_S(HID_KEY_SEMICOLON);     // Ö
        case 0xE4: return KC(HID_KEY_APOSTROPHE);      // ä
        case 0xC4: return KC_S(HID_KEY_APOSTROPHE);    // Ä

        case '$':  return KC(HID_KEY_BACKSLASH);       // $ on Swiss keyboard
        case 0xA3: return KC_S(HID_KEY_BACKSLASH);     // £

        case '<':  return KC(HID_KEY_EUROPE_2);        // < (key left of Y)
        case '>':  return KC_S(HID_KEY_EUROPE_2);      // >

        case ',':  return KC(HID_KEY_COMMA);
        case ';':  return KC_S(HID_KEY_COMMA);
        case '.':  return KC(HID_KEY_PERIOD);
        case ':':  return KC_S(HID_KEY_PERIOD);
        case '-':  return KC(HID_KEY_SLASH);
        case '_':  return KC_S(HID_KEY_SLASH);

        // AltGr combinations for Swiss German
        case '@':  return KC_A(HID_KEY_2);
        case '#':  return KC_A(HID_KEY_3);
        case 0xAC: return KC_A(HID_KEY_6);             // ¬
        case 0xA6: return KC_A(HID_KEY_7);             // ¦
        case 0xA2: return KC_A(HID_KEY_8);             // ¢
        case '[':  return KC_A(HID_KEY_BRACKET_LEFT);
        case ']':  return KC_A(HID_KEY_BRACKET_RIGHT);
        case '{':  return KC_A(HID_KEY_APOSTROPHE);
        case '}':  return KC_A(HID_KEY_BACKSLASH);
        case '\\': return KC_A(HID_KEY_EUROPE_2);
        case '|':  return KC_A(HID_KEY_7);
        case '~':  return KC_A(HID_KEY_EQUAL);

        default:   return 0;
    }
}

// ============================================================================
// German Layout (QWERTZ)
// ============================================================================
static uint16_t layout_de_lookup(uint32_t cp) {
    // Letters - Z and Y swapped
    if (cp >= 'a' && cp <= 'z') {
        if (cp == 'y') return KC(HID_KEY_Z);
        if (cp == 'z') return KC(HID_KEY_Y);
        return KC(HID_KEY_A + (cp - 'a'));
    }
    if (cp >= 'A' && cp <= 'Z') {
        if (cp == 'Y') return KC_S(HID_KEY_Z);
        if (cp == 'Z') return KC_S(HID_KEY_Y);
        return KC_S(HID_KEY_A + (cp - 'A'));
    }

    if (cp >= '1' && cp <= '9') return KC(HID_KEY_1 + (cp - '1'));
    if (cp == '0') return KC(HID_KEY_0);

    switch (cp) {
        case ' ':  return KC(HID_KEY_SPACE);
        case '\n': return KC(HID_KEY_ENTER);
        case '\t': return KC(HID_KEY_TAB);

        // German shifted numbers
        case '!':  return KC_S(HID_KEY_1);
        case '"':  return KC_S(HID_KEY_2);
        case 0xA7: return KC_S(HID_KEY_3);  // §
        case '$':  return KC_S(HID_KEY_4);
        case '%':  return KC_S(HID_KEY_5);
        case '&':  return KC_S(HID_KEY_6);
        case '/':  return KC_S(HID_KEY_7);
        case '(':  return KC_S(HID_KEY_8);
        case ')':  return KC_S(HID_KEY_9);
        case '=':  return KC_S(HID_KEY_0);

        // German layout special keys
        case 0xDF: return KC(HID_KEY_MINUS);           // ß
        case '?':  return KC_S(HID_KEY_MINUS);
        case 0xB4: return KC(HID_KEY_EQUAL);           // ´ (acute accent)
        case '`':  return KC_S(HID_KEY_EQUAL);

        // Umlauts
        case 0xFC: return KC(HID_KEY_BRACKET_LEFT);    // ü
        case 0xDC: return KC_S(HID_KEY_BRACKET_LEFT);  // Ü
        case '+':  return KC(HID_KEY_BRACKET_RIGHT);
        case '*':  return KC_S(HID_KEY_BRACKET_RIGHT);
        case 0xF6: return KC(HID_KEY_SEMICOLON);       // ö
        case 0xD6: return KC_S(HID_KEY_SEMICOLON);     // Ö
        case 0xE4: return KC(HID_KEY_APOSTROPHE);      // ä
        case 0xC4: return KC_S(HID_KEY_APOSTROPHE);    // Ä

        case '#':  return KC(HID_KEY_BACKSLASH);
        case '\'': return KC_S(HID_KEY_BACKSLASH);

        case '<':  return KC(HID_KEY_EUROPE_2);
        case '>':  return KC_S(HID_KEY_EUROPE_2);
        case '|':  return KC_A(HID_KEY_EUROPE_2);

        case ',':  return KC(HID_KEY_COMMA);
        case ';':  return KC_S(HID_KEY_COMMA);
        case '.':  return KC(HID_KEY_PERIOD);
        case ':':  return KC_S(HID_KEY_PERIOD);
        case '-':  return KC(HID_KEY_SLASH);
        case '_':  return KC_S(HID_KEY_SLASH);

        // AltGr
        case '@':  return KC_A(HID_KEY_Q);
        case 0x20AC: return KC_A(HID_KEY_E);           // €
        case '{':  return KC_A(HID_KEY_7);
        case '[':  return KC_A(HID_KEY_8);
        case ']':  return KC_A(HID_KEY_9);
        case '}':  return KC_A(HID_KEY_0);
        case '\\': return KC_A(HID_KEY_MINUS);
        case '~':  return KC_A(HID_KEY_BRACKET_RIGHT);

        default:   return 0;
    }
}

// ============================================================================
// French Layout (AZERTY)
// ============================================================================
static uint16_t layout_fr_lookup(uint32_t cp) {
    // AZERTY letter mapping
    if (cp >= 'a' && cp <= 'z') {
        switch (cp) {
            case 'a': return KC(HID_KEY_Q);
            case 'q': return KC(HID_KEY_A);
            case 'z': return KC(HID_KEY_W);
            case 'w': return KC(HID_KEY_Z);
            case 'm': return KC(HID_KEY_SEMICOLON);
            default:  return KC(HID_KEY_A + (cp - 'a'));
        }
    }
    if (cp >= 'A' && cp <= 'Z') {
        switch (cp) {
            case 'A': return KC_S(HID_KEY_Q);
            case 'Q': return KC_S(HID_KEY_A);
            case 'Z': return KC_S(HID_KEY_W);
            case 'W': return KC_S(HID_KEY_Z);
            case 'M': return KC_S(HID_KEY_SEMICOLON);
            default:  return KC_S(HID_KEY_A + (cp - 'A'));
        }
    }

    // French numbers need shift!
    switch (cp) {
        case '&':  return KC(HID_KEY_1);
        case 0xE9: return KC(HID_KEY_2);  // é
        case '"':  return KC(HID_KEY_3);
        case '\'': return KC(HID_KEY_4);
        case '(':  return KC(HID_KEY_5);
        case '-':  return KC(HID_KEY_6);
        case 0xE8: return KC(HID_KEY_7);  // è
        case '_':  return KC(HID_KEY_8);
        case 0xE7: return KC(HID_KEY_9);  // ç
        case 0xE0: return KC(HID_KEY_0);  // à

        // Shifted numbers give actual numbers
        case '1':  return KC_S(HID_KEY_1);
        case '2':  return KC_S(HID_KEY_2);
        case '3':  return KC_S(HID_KEY_3);
        case '4':  return KC_S(HID_KEY_4);
        case '5':  return KC_S(HID_KEY_5);
        case '6':  return KC_S(HID_KEY_6);
        case '7':  return KC_S(HID_KEY_7);
        case '8':  return KC_S(HID_KEY_8);
        case '9':  return KC_S(HID_KEY_9);
        case '0':  return KC_S(HID_KEY_0);

        case ' ':  return KC(HID_KEY_SPACE);
        case '\n': return KC(HID_KEY_ENTER);
        case '\t': return KC(HID_KEY_TAB);

        case ')':  return KC(HID_KEY_MINUS);
        case 0xB0: return KC_S(HID_KEY_MINUS);  // °
        case '=':  return KC(HID_KEY_EQUAL);
        case '+':  return KC_S(HID_KEY_EQUAL);

        case '^':  return KC(HID_KEY_BRACKET_LEFT);
        case '$':  return KC(HID_KEY_BRACKET_RIGHT);
        case 0xF9: return KC(HID_KEY_APOSTROPHE);  // ù
        case '%':  return KC_S(HID_KEY_APOSTROPHE);
        case '*':  return KC(HID_KEY_BACKSLASH);
        case 0xB5: return KC_S(HID_KEY_BACKSLASH);  // µ

        case '<':  return KC(HID_KEY_EUROPE_2);
        case '>':  return KC_S(HID_KEY_EUROPE_2);

        case ',':  return KC(HID_KEY_M);
        case '?':  return KC_S(HID_KEY_M);
        case ';':  return KC(HID_KEY_COMMA);
        case '.':  return KC_S(HID_KEY_COMMA);
        case ':':  return KC(HID_KEY_PERIOD);
        case '/':  return KC_S(HID_KEY_PERIOD);
        case '!':  return KC(HID_KEY_SLASH);
        case 0xA7: return KC_S(HID_KEY_SLASH);  // §

        // AltGr
        case '~':  return KC_A(HID_KEY_2);
        case '#':  return KC_A(HID_KEY_3);
        case '{':  return KC_A(HID_KEY_4);
        case '[':  return KC_A(HID_KEY_5);
        case '|':  return KC_A(HID_KEY_6);
        case '`':  return KC_A(HID_KEY_7);
        case '\\': return KC_A(HID_KEY_8);
        case ']':  return KC_A(HID_KEY_MINUS);
        case '}':  return KC_A(HID_KEY_EQUAL);
        case '@':  return KC_A(HID_KEY_0);
        case 0x20AC: return KC_A(HID_KEY_E);  // €

        default:   return 0;
    }
}

// ============================================================================
// UK English Layout
// ============================================================================
static uint16_t layout_uk_lookup(uint32_t cp) {
    // Same as US for most letters
    if (cp >= 'a' && cp <= 'z') return KC(HID_KEY_A + (cp - 'a'));
    if (cp >= 'A' && cp <= 'Z') return KC_S(HID_KEY_A + (cp - 'A'));
    if (cp >= '1' && cp <= '9') return KC(HID_KEY_1 + (cp - '1'));
    if (cp == '0') return KC(HID_KEY_0);

    switch (cp) {
        case ' ':  return KC(HID_KEY_SPACE);
        case '\n': return KC(HID_KEY_ENTER);
        case '\t': return KC(HID_KEY_TAB);

        // UK differs from US here
        case '"':  return KC_S(HID_KEY_2);
        case 0xA3: return KC_S(HID_KEY_3);  // £
        case '#':  return KC(HID_KEY_BACKSLASH);
        case '~':  return KC_S(HID_KEY_BACKSLASH);
        case '@':  return KC_S(HID_KEY_APOSTROPHE);
        case '\'': return KC(HID_KEY_APOSTROPHE);

        // Rest similar to US
        case '!':  return KC_S(HID_KEY_1);
        case '$':  return KC_S(HID_KEY_4);
        case '%':  return KC_S(HID_KEY_5);
        case '^':  return KC_S(HID_KEY_6);
        case '&':  return KC_S(HID_KEY_7);
        case '*':  return KC_S(HID_KEY_8);
        case '(':  return KC_S(HID_KEY_9);
        case ')':  return KC_S(HID_KEY_0);
        case '-':  return KC(HID_KEY_MINUS);
        case '_':  return KC_S(HID_KEY_MINUS);
        case '=':  return KC(HID_KEY_EQUAL);
        case '+':  return KC_S(HID_KEY_EQUAL);
        case '[':  return KC(HID_KEY_BRACKET_LEFT);
        case ']':  return KC(HID_KEY_BRACKET_RIGHT);
        case '{':  return KC_S(HID_KEY_BRACKET_LEFT);
        case '}':  return KC_S(HID_KEY_BRACKET_RIGHT);
        case '\\': return KC(HID_KEY_EUROPE_1);
        case '|':  return KC_S(HID_KEY_EUROPE_1);
        case ';':  return KC(HID_KEY_SEMICOLON);
        case ':':  return KC_S(HID_KEY_SEMICOLON);
        case '`':  return KC(HID_KEY_GRAVE);
        case 0xAC: return KC_S(HID_KEY_GRAVE);  // ¬
        case ',':  return KC(HID_KEY_COMMA);
        case '.':  return KC(HID_KEY_PERIOD);
        case '<':  return KC_S(HID_KEY_COMMA);
        case '>':  return KC_S(HID_KEY_PERIOD);
        case '/':  return KC(HID_KEY_SLASH);
        case '?':  return KC_S(HID_KEY_SLASH);

        // AltGr
        case 0x20AC: return KC_A(HID_KEY_4);  // €
        case 0xE9:   return KC_A(HID_KEY_E);  // é
        case 0xFA:   return KC_A(HID_KEY_U);  // ú
        case 0xED:   return KC_A(HID_KEY_I);  // í
        case 0xF3:   return KC_A(HID_KEY_O);  // ó
        case 0xE1:   return KC_A(HID_KEY_A);  // á

        default:   return 0;
    }
}

// ============================================================================
// Spanish Layout
// ============================================================================
static uint16_t layout_es_lookup(uint32_t cp) {
    if (cp >= 'a' && cp <= 'z') return KC(HID_KEY_A + (cp - 'a'));
    if (cp >= 'A' && cp <= 'Z') return KC_S(HID_KEY_A + (cp - 'A'));
    if (cp >= '1' && cp <= '9') return KC(HID_KEY_1 + (cp - '1'));
    if (cp == '0') return KC(HID_KEY_0);

    switch (cp) {
        case ' ':  return KC(HID_KEY_SPACE);
        case '\n': return KC(HID_KEY_ENTER);
        case '\t': return KC(HID_KEY_TAB);

        // Spanish shifted numbers
        case '!':  return KC_S(HID_KEY_1);
        case '"':  return KC_S(HID_KEY_2);
        case 0xB7: return KC_S(HID_KEY_3);  // ·
        case '$':  return KC_S(HID_KEY_4);
        case '%':  return KC_S(HID_KEY_5);
        case '&':  return KC_S(HID_KEY_6);
        case '/':  return KC_S(HID_KEY_7);
        case '(':  return KC_S(HID_KEY_8);
        case ')':  return KC_S(HID_KEY_9);
        case '=':  return KC_S(HID_KEY_0);

        case '\'': return KC(HID_KEY_MINUS);
        case '?':  return KC_S(HID_KEY_MINUS);
        case 0xBF: return KC_S(HID_KEY_EQUAL);  // ¿
        case 0xA1: return KC(HID_KEY_EQUAL);    // ¡

        case '`':  return KC(HID_KEY_BRACKET_LEFT);
        case '^':  return KC_S(HID_KEY_BRACKET_LEFT);
        case '+':  return KC(HID_KEY_BRACKET_RIGHT);
        case '*':  return KC_S(HID_KEY_BRACKET_RIGHT);

        case 0xF1: return KC(HID_KEY_SEMICOLON);    // ñ
        case 0xD1: return KC_S(HID_KEY_SEMICOLON);  // Ñ

        case 0xB4: return KC(HID_KEY_APOSTROPHE);   // ´
        case 0xA8: return KC_S(HID_KEY_APOSTROPHE); // ¨

        case 0xE7: return KC(HID_KEY_BACKSLASH);    // ç
        case 0xC7: return KC_S(HID_KEY_BACKSLASH);  // Ç

        case '<':  return KC(HID_KEY_EUROPE_2);
        case '>':  return KC_S(HID_KEY_EUROPE_2);

        case ',':  return KC(HID_KEY_COMMA);
        case ';':  return KC_S(HID_KEY_COMMA);
        case '.':  return KC(HID_KEY_PERIOD);
        case ':':  return KC_S(HID_KEY_PERIOD);
        case '-':  return KC(HID_KEY_SLASH);
        case '_':  return KC_S(HID_KEY_SLASH);

        // AltGr
        case '|':  return KC_A(HID_KEY_1);
        case '@':  return KC_A(HID_KEY_2);
        case '#':  return KC_A(HID_KEY_3);
        case '~':  return KC_A(HID_KEY_4);
        case 0x20AC: return KC_A(HID_KEY_5);  // €
        case '[':  return KC_A(HID_KEY_BRACKET_LEFT);
        case ']':  return KC_A(HID_KEY_BRACKET_RIGHT);
        case '{':  return KC_A(HID_KEY_APOSTROPHE);
        case '}':  return KC_A(HID_KEY_BACKSLASH);
        case '\\': return KC_A(HID_KEY_GRAVE);

        default:   return 0;
    }
}

// ============================================================================
// Italian Layout
// ============================================================================
static uint16_t layout_it_lookup(uint32_t cp) {
    if (cp >= 'a' && cp <= 'z') return KC(HID_KEY_A + (cp - 'a'));
    if (cp >= 'A' && cp <= 'Z') return KC_S(HID_KEY_A + (cp - 'A'));
    if (cp >= '1' && cp <= '9') return KC(HID_KEY_1 + (cp - '1'));
    if (cp == '0') return KC(HID_KEY_0);

    switch (cp) {
        case ' ':  return KC(HID_KEY_SPACE);
        case '\n': return KC(HID_KEY_ENTER);
        case '\t': return KC(HID_KEY_TAB);

        case '!':  return KC_S(HID_KEY_1);
        case '"':  return KC_S(HID_KEY_2);
        case 0xA3: return KC_S(HID_KEY_3);  // £
        case '$':  return KC_S(HID_KEY_4);
        case '%':  return KC_S(HID_KEY_5);
        case '&':  return KC_S(HID_KEY_6);
        case '/':  return KC_S(HID_KEY_7);
        case '(':  return KC_S(HID_KEY_8);
        case ')':  return KC_S(HID_KEY_9);
        case '=':  return KC_S(HID_KEY_0);

        case '\'': return KC(HID_KEY_MINUS);
        case '?':  return KC_S(HID_KEY_MINUS);
        case 0xEC: return KC(HID_KEY_EQUAL);   // ì
        case '^':  return KC_S(HID_KEY_EQUAL);

        case 0xE8: return KC(HID_KEY_BRACKET_LEFT);   // è
        case 0xE9: return KC_S(HID_KEY_BRACKET_LEFT); // é
        case '+':  return KC(HID_KEY_BRACKET_RIGHT);
        case '*':  return KC_S(HID_KEY_BRACKET_RIGHT);

        case 0xF2: return KC(HID_KEY_SEMICOLON);      // ò
        case 0xE7: return KC_S(HID_KEY_SEMICOLON);    // ç
        case 0xE0: return KC(HID_KEY_APOSTROPHE);     // à
        case 0xB0: return KC_S(HID_KEY_APOSTROPHE);   // °
        case 0xF9: return KC(HID_KEY_BACKSLASH);      // ù
        case 0xA7: return KC_S(HID_KEY_BACKSLASH);    // §

        case '<':  return KC(HID_KEY_EUROPE_2);
        case '>':  return KC_S(HID_KEY_EUROPE_2);

        case ',':  return KC(HID_KEY_COMMA);
        case ';':  return KC_S(HID_KEY_COMMA);
        case '.':  return KC(HID_KEY_PERIOD);
        case ':':  return KC_S(HID_KEY_PERIOD);
        case '-':  return KC(HID_KEY_SLASH);
        case '_':  return KC_S(HID_KEY_SLASH);

        // AltGr
        case '@':  return KC_A(HID_KEY_SEMICOLON);
        case '#':  return KC_A(HID_KEY_APOSTROPHE);
        case '[':  return KC_A(HID_KEY_BRACKET_LEFT);
        case ']':  return KC_A(HID_KEY_BRACKET_RIGHT);
        case '{':  return KC_A(HID_KEY_7);
        case '}':  return KC_A(HID_KEY_0);
        case 0x20AC: return KC_A(HID_KEY_E);  // €

        default:   return 0;
    }
}

// ============================================================================
// Layout Dispatcher
// ============================================================================
static uint16_t (*s_layout_lookup[])(uint32_t) = {
    [LAYOUT_US]    = layout_us_lookup,
    [LAYOUT_CH_DE] = layout_ch_de_lookup,
    [LAYOUT_DE]    = layout_de_lookup,
    [LAYOUT_FR]    = layout_fr_lookup,
    [LAYOUT_UK]    = layout_uk_lookup,
    [LAYOUT_ES]    = layout_es_lookup,
    [LAYOUT_IT]    = layout_it_lookup,
};

// ============================================================================
// Public API
// ============================================================================

esp_err_t keyboard_layout_init(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        uint8_t layout = 0;
        err = nvs_get_u8(nvs, NVS_KEY_LAYOUT, &layout);
        if (err == ESP_OK && layout < LAYOUT_COUNT) {
            s_current_layout = (keyboard_layout_t)layout;
            ESP_LOGI(TAG, "Loaded keyboard layout: %s", s_layout_info[s_current_layout].name);
        }
        nvs_close(nvs);
    }

    if (err != ESP_OK) {
        s_current_layout = LAYOUT_CH_DE;  // Default to Swiss German
        ESP_LOGI(TAG, "Using default layout: %s", s_layout_info[s_current_layout].name);
    }

    return ESP_OK;
}

keyboard_layout_t keyboard_layout_get(void)
{
    return s_current_layout;
}

esp_err_t keyboard_layout_set(keyboard_layout_t layout)
{
    if (layout >= LAYOUT_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    s_current_layout = layout;

    // Save to NVS
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        err = nvs_set_u8(nvs, NVS_KEY_LAYOUT, (uint8_t)layout);
        if (err == ESP_OK) {
            err = nvs_commit(nvs);
        }
        nvs_close(nvs);
    }

    ESP_LOGI(TAG, "Keyboard layout set to: %s", s_layout_info[layout].name);
    return err;
}

esp_err_t keyboard_layout_set_by_code(const char *code)
{
    for (int i = 0; i < LAYOUT_COUNT; i++) {
        if (strcmp(s_layout_info[i].code, code) == 0) {
            return keyboard_layout_set((keyboard_layout_t)i);
        }
    }
    return ESP_ERR_NOT_FOUND;
}

const keyboard_layout_info_t *keyboard_layout_get_info(keyboard_layout_t layout)
{
    if (layout >= LAYOUT_COUNT) {
        return NULL;
    }
    return &s_layout_info[layout];
}

const keyboard_layout_info_t *keyboard_layout_get_all(int *count)
{
    if (count) {
        *count = LAYOUT_COUNT;
    }
    return s_layout_info;
}

uint16_t keyboard_layout_char_to_keycode(uint32_t codepoint)
{
    if (s_current_layout >= LAYOUT_COUNT) {
        return 0;
    }
    return s_layout_lookup[s_current_layout](codepoint);
}

// UTF-8 decoder helper
static int utf8_decode(const char *str, uint32_t *codepoint)
{
    uint8_t c = (uint8_t)str[0];

    if ((c & 0x80) == 0) {
        *codepoint = c;
        return 1;
    } else if ((c & 0xE0) == 0xC0) {
        *codepoint = ((c & 0x1F) << 6) | (str[1] & 0x3F);
        return 2;
    } else if ((c & 0xF0) == 0xE0) {
        *codepoint = ((c & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
        return 3;
    } else if ((c & 0xF8) == 0xF0) {
        *codepoint = ((c & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
        return 4;
    }

    *codepoint = 0;
    return 1;
}

int keyboard_layout_string_to_keycodes(const char *utf8_str, keycode_callback_t callback, void *ctx)
{
    int count = 0;
    const char *p = utf8_str;

    while (*p) {
        uint32_t cp;
        int len = utf8_decode(p, &cp);

        uint16_t keydata = keyboard_layout_char_to_keycode(cp);
        if (keydata != 0) {
            uint8_t keycode = keydata & 0xFF;
            uint8_t modifiers = (keydata >> 8) & 0xFF;
            callback(keycode, modifiers, ctx);
            count++;
        }

        p += len;
    }

    return count;
}
