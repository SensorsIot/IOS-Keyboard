#include "ble_gatt.h"
#include "config.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "ble_gatt";

// Nordic UART Service UUIDs (128-bit)
// Service: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
// RX Char: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E (Write)
// TX Char: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E (Notify)
static const ble_uuid128_t nus_service_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);

static const ble_uuid128_t nus_rx_char_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);

static const ble_uuid128_t nus_tx_char_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

// Module state
static ble_gatt_state_t s_state = BLE_STATE_IDLE;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_tx_attr_handle = 0;
static ble_gatt_rx_callback_t s_rx_callback = NULL;
static bool s_initialized = false;

// Forward declarations
static int ble_gap_event(struct ble_gap_event *event, void *arg);
static void ble_on_sync(void);
static void ble_host_task(void *param);

// GATT access callback for RX characteristic (write from client)
static int gatt_chr_access_rx(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        struct os_mbuf *om = ctxt->om;
        uint16_t len = OS_MBUF_PKTLEN(om);

        if (len > 0) {
            uint8_t buf[256];
            uint16_t copy_len = len < sizeof(buf) ? len : sizeof(buf);
            os_mbuf_copydata(om, 0, copy_len, buf);

            ESP_LOGI(TAG, "RX: %d bytes", copy_len);
            ESP_LOG_BUFFER_HEX(TAG, buf, copy_len);

            if (s_rx_callback != NULL) {
                ESP_LOGI(TAG, "Calling RX callback");
                s_rx_callback(buf, copy_len);
            } else {
                ESP_LOGW(TAG, "No RX callback registered!");
            }
        } else {
            ESP_LOGW(TAG, "RX: empty write received");
        }
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// GATT access callback for TX characteristic (notify to client)
static int gatt_chr_access_tx(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    // TX is notify-only, no read/write handlers needed
    return 0;
}

// GATT service definition
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &nus_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // RX Characteristic (Write from client to device)
                .uuid = &nus_rx_char_uuid.u,
                .access_cb = gatt_chr_access_rx,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                // TX Characteristic (Notify from device to client)
                .uuid = &nus_tx_char_uuid.u,
                .access_cb = gatt_chr_access_tx,
                .val_handle = &s_tx_attr_handle,
                .flags = BLE_GATT_CHR_F_NOTIFY,
            },
            {0}, // Terminator
        },
    },
    {0}, // Terminator
};

// Start advertising
static void ble_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    struct ble_hs_adv_fields rsp_fields;
    int rc;

    // Main advertising data: flags + name
    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)CONFIG_BLE_DEVICE_NAME;
    fields.name_len = strlen(CONFIG_BLE_DEVICE_NAME);
    fields.name_is_complete = 1;

    ESP_LOGI(TAG, "Setting adv data: name='%s' len=%d", CONFIG_BLE_DEVICE_NAME, fields.name_len);

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set adv fields: %d", rc);
        return;
    }

    // Scan response data: 128-bit service UUID
    memset(&rsp_fields, 0, sizeof(rsp_fields));
    rsp_fields.uuids128 = (ble_uuid128_t *)&nus_service_uuid;
    rsp_fields.num_uuids128 = 1;
    rsp_fields.uuids128_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set scan rsp fields: %d", rc);
        return;
    }

    ESP_LOGI(TAG, "Adv and scan rsp data set successfully");

    // Advertising parameters
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;  // Connectable
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;  // General discoverable

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start advertising: %d", rc);
        return;
    }

    s_state = BLE_STATE_ADVERTISING;
    ESP_LOGI(TAG, "Advertising started as '%s'", CONFIG_BLE_DEVICE_NAME);
}

// GAP event handler
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    ESP_LOGI(TAG, "GAP event: type=%d", event->type);

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "GAP_EVENT_CONNECT: status=%d", event->connect.status);
            if (event->connect.status == 0) {
                s_conn_handle = event->connect.conn_handle;
                s_state = BLE_STATE_CONNECTED;
                ESP_LOGI(TAG, "Client connected (handle=%d)", s_conn_handle);
            } else {
                ESP_LOGW(TAG, "Connection failed: %d", event->connect.status);
                ble_advertise();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "GAP_EVENT_DISCONNECT: reason=%d", event->disconnect.reason);
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            s_state = BLE_STATE_IDLE;
            ble_advertise();
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "GAP_EVENT_ADV_COMPLETE");
            ble_advertise();
            break;

        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "GAP_EVENT_MTU: value=%d", event->mtu.value);
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            ESP_LOGI(TAG, "GAP_EVENT_SUBSCRIBE: attr_handle=%d, notify=%d, indicate=%d",
                     event->subscribe.attr_handle,
                     event->subscribe.cur_notify,
                     event->subscribe.cur_indicate);
            break;

        case BLE_GAP_EVENT_NOTIFY_TX:
            ESP_LOGI(TAG, "GAP_EVENT_NOTIFY_TX: status=%d", event->notify_tx.status);
            break;

        default:
            ESP_LOGI(TAG, "Unhandled GAP event: %d", event->type);
            break;
    }
    return 0;
}

// Called when NimBLE stack is synchronized
static void ble_on_sync(void)
{
    int rc;
    uint8_t addr_type;

    // Use public address
    rc = ble_hs_id_infer_auto(0, &addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to infer address type: %d", rc);
        return;
    }

    // Start advertising
    ble_advertise();
}

// NimBLE host task
static void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "NimBLE host task started");
    nimble_port_run();  // This returns only when nimble_port_stop() is called
    nimble_port_freertos_deinit();
}

esp_err_t ble_gatt_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing BLE GATT");

    // Initialize NimBLE
    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NimBLE: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure NimBLE host
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = NULL;

    // Initialize GATT services
    ble_svc_gap_init();
    ble_svc_gatt_init();

    // Configure GATT services
    int rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to count GATT services: %d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to add GATT services: %d", rc);
        return ESP_FAIL;
    }

    // Set device name
    rc = ble_svc_gap_device_name_set(CONFIG_BLE_DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGW(TAG, "Failed to set device name: %d", rc);
    }

    s_initialized = true;
    ESP_LOGI(TAG, "BLE GATT initialized");
    return ESP_OK;
}

esp_err_t ble_gatt_start(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Start NimBLE host task
    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(TAG, "BLE GATT started");
    return ESP_OK;
}

esp_err_t ble_gatt_stop(void)
{
    if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }

    int rc = ble_gap_adv_stop();
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGW(TAG, "Failed to stop advertising: %d", rc);
    }

    s_state = BLE_STATE_IDLE;
    ESP_LOGI(TAG, "BLE GATT stopped");
    return ESP_OK;
}

bool ble_gatt_is_connected(void)
{
    return s_state == BLE_STATE_CONNECTED;
}

ble_gatt_state_t ble_gatt_get_state(void)
{
    return s_state;
}

void ble_gatt_set_rx_callback(ble_gatt_rx_callback_t callback)
{
    s_rx_callback = callback;
}

esp_err_t ble_gatt_send(const uint8_t *data, size_t len)
{
    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return ESP_ERR_INVALID_STATE;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (om == NULL) {
        return ESP_ERR_NO_MEM;
    }

    int rc = ble_gatts_notify_custom(s_conn_handle, s_tx_attr_handle, om);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to send notification: %d", rc);
        return ESP_FAIL;
    }

    return ESP_OK;
}
