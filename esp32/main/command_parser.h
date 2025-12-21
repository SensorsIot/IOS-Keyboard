#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

/**
 * Initialize the command parser
 */
esp_err_t command_parser_init(void);

/**
 * Process a received command packet
 * Called by BLE GATT when data is received on RX characteristic
 *
 * Packet format:
 * - 0x01 <count>  : Send <count> backspace keystrokes
 * - 0x02 <text>   : Type the text characters
 * - 0x03          : Send enter key
 *
 * @param data Pointer to received data
 * @param len Length of received data
 */
void command_parser_process(const uint8_t *data, size_t len);

#endif // COMMAND_PARSER_H
