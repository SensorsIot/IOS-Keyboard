#ifndef BLE_GATT_H
#define BLE_GATT_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * BLE connection state
 */
typedef enum {
    BLE_STATE_IDLE,
    BLE_STATE_ADVERTISING,
    BLE_STATE_CONNECTED,
} ble_gatt_state_t;

/**
 * Callback type for received data on RX characteristic
 */
typedef void (*ble_gatt_rx_callback_t)(const uint8_t *data, size_t len);

/**
 * Initialize BLE GATT server with Nordic UART Service
 */
esp_err_t ble_gatt_init(void);

/**
 * Start BLE advertising
 */
esp_err_t ble_gatt_start(void);

/**
 * Stop BLE advertising and disconnect
 */
esp_err_t ble_gatt_stop(void);

/**
 * Check if a BLE client is connected
 */
bool ble_gatt_is_connected(void);

/**
 * Get current BLE state
 */
ble_gatt_state_t ble_gatt_get_state(void);

/**
 * Set callback for received data
 * @param callback Function to call when data is received on RX characteristic
 */
void ble_gatt_set_rx_callback(ble_gatt_rx_callback_t callback);

/**
 * Send data to connected client via TX characteristic (notify)
 * @param data Data to send
 * @param len Length of data
 * @return ESP_OK on success
 */
esp_err_t ble_gatt_send(const uint8_t *data, size_t len);

#endif // BLE_GATT_H
