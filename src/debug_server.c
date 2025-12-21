#include "debug_server.h"
#include "wifi_manager.h"
#include "ota_handler.h"
#include "config.h"

#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "cJSON.h"

static const char *TAG = "debug_srv";
static httpd_handle_t s_server = NULL;

// Log buffer
#define LOG_MSG_MAX_LEN 128
static char s_log_buffer[CONFIG_LOG_BUFFER_SIZE][LOG_MSG_MAX_LEN];
static int s_log_index = 0;
static int s_log_count = 0;
static SemaphoreHandle_t s_log_mutex = NULL;

// Boot time for uptime calculation
static int64_t s_boot_time = 0;

// Embedded HTML for debug dashboard
static const char DEBUG_HTML[] =
"<!DOCTYPE html>"
"<html><head>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>IOS-Keyboard Debug</title>"
"<style>"
"body{font-family:monospace;margin:20px;background:#0a0a0a;color:#0f0;}"
"h1{text-align:center;}"
".container{max-width:800px;margin:0 auto;}"
".card{background:#111;padding:15px;border:1px solid #0f0;border-radius:5px;margin:10px 0;}"
".card h3{margin-top:0;border-bottom:1px solid #0f0;padding-bottom:5px;}"
".status-row{display:flex;justify-content:space-between;padding:5px 0;}"
".status-label{color:#888;}"
".status-value{color:#0f0;}"
"button{padding:10px 20px;background:#0f0;color:#000;border:none;"
"border-radius:3px;cursor:pointer;margin:5px;font-family:monospace;}"
"button:hover{background:#0c0;}"
"button.danger{background:#f00;color:#fff;}"
"button.danger:hover{background:#c00;}"
"input{padding:10px;background:#000;color:#0f0;border:1px solid #0f0;"
"border-radius:3px;width:100%;box-sizing:border-box;font-family:monospace;}"
".logs{background:#000;padding:10px;border:1px solid #333;border-radius:3px;"
"height:300px;overflow-y:auto;font-size:12px;}"
".log-entry{padding:2px 0;border-bottom:1px solid #222;}"
".progress{background:#333;border-radius:3px;height:20px;margin:10px 0;}"
".progress-bar{background:#0f0;height:100%;border-radius:3px;transition:width 0.3s;}"
".hidden{display:none;}"
"</style>"
"</head><body>"
"<div class='container'>"
"<h1>IOS-Keyboard Debug</h1>"
"<div class='card'>"
"<h3>System Status</h3>"
"<div class='status-row'><span class='status-label'>Version:</span><span class='status-value' id='version'>-</span></div>"
"<div class='status-row'><span class='status-label'>Uptime:</span><span class='status-value' id='uptime'>-</span></div>"
"<div class='status-row'><span class='status-label'>WiFi SSID:</span><span class='status-value' id='ssid'>-</span></div>"
"<div class='status-row'><span class='status-label'>IP Address:</span><span class='status-value' id='ip'>-</span></div>"
"<div class='status-row'><span class='status-label'>RSSI:</span><span class='status-value' id='rssi'>-</span></div>"
"<div class='status-row'><span class='status-label'>Free Heap:</span><span class='status-value' id='heap'>-</span></div>"
"</div>"
"<div class='card'>"
"<h3>OTA Update</h3>"
"<input type='text' id='otaUrl' placeholder='http://server/firmware.bin'>"
"<div style='margin-top:10px;'>"
"<button onclick='startOta()'>Start OTA Update</button>"
"</div>"
"<div id='otaProgress' class='hidden'>"
"<div class='progress'><div class='progress-bar' id='otaBar' style='width:0%'></div></div>"
"<div id='otaStatus'>Idle</div>"
"</div>"
"</div>"
"<div class='card'>"
"<h3>Actions</h3>"
"<button onclick='typeTest()'>Type Test</button>"
"<button onclick='location.reload()'>Refresh</button>"
"<button class='danger' onclick='resetWifi()'>Reset WiFi</button>"
"<button class='danger' onclick='reboot()'>Reboot</button>"
"</div>"
"<div class='card'>"
"<h3>Logs</h3>"
"<button onclick='refreshLogs()'>Refresh Logs</button>"
"<div class='logs' id='logs'></div>"
"</div>"
"</div>"
"<script>"
"function updateStatus(){"
"fetch('/status').then(r=>r.json()).then(d=>{"
"document.getElementById('version').textContent=d.version;"
"document.getElementById('uptime').textContent=formatUptime(d.uptime);"
"document.getElementById('ssid').textContent=d.ssid;"
"document.getElementById('ip').textContent=d.ip;"
"document.getElementById('rssi').textContent=d.rssi+' dBm';"
"document.getElementById('heap').textContent=Math.round(d.heap/1024)+' KB';"
"if(d.ota_status!=='idle'){"
"document.getElementById('otaProgress').classList.remove('hidden');"
"document.getElementById('otaBar').style.width=d.ota_progress+'%';"
"document.getElementById('otaStatus').textContent=d.ota_status+' ('+d.ota_progress+'%)';"
"}}).catch(e=>console.error(e));}"
"function formatUptime(s){let h=Math.floor(s/3600);let m=Math.floor((s%3600)/60);return h+'h '+m+'m';}"
"function startOta(){"
"let url=document.getElementById('otaUrl').value;"
"if(!url){alert('Enter firmware URL');return;}"
"fetch('/ota',{method:'POST',headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({url:url})}).then(r=>r.json()).then(d=>{"
"if(d.success){document.getElementById('otaProgress').classList.remove('hidden');}"
"else{alert('OTA failed: '+d.message);}});}"
"function typeTest(){"
"fetch('/type',{method:'POST'}).then(r=>r.json()).then(d=>alert(d.message));}"
"function resetWifi(){"
"if(confirm('Clear WiFi credentials and reboot?')){"
"fetch('/reset-wifi',{method:'POST'}).then(()=>alert('Rebooting...'));}}"
"function reboot(){"
"if(confirm('Reboot device?')){"
"fetch('/reboot',{method:'POST'}).then(()=>alert('Rebooting...'));}}"
"function refreshLogs(){"
"fetch('/logs').then(r=>r.json()).then(d=>{"
"let html='';d.logs.forEach(l=>{html+='<div class=\"log-entry\">'+l+'</div>';});"
"document.getElementById('logs').innerHTML=html;});}"
"updateStatus();refreshLogs();"
"setInterval(updateStatus,5000);"
"</script>"
"</body></html>";

// Add log message to buffer
void debug_server_log(const char *format, ...)
{
    if (s_log_mutex == NULL) return;

    va_list args;
    va_start(args, format);

    xSemaphoreTake(s_log_mutex, portMAX_DELAY);

    // Get timestamp
    int64_t now = esp_timer_get_time() / 1000000;
    int len = snprintf(s_log_buffer[s_log_index], LOG_MSG_MAX_LEN,
                       "[%lld] ", now);
    vsnprintf(s_log_buffer[s_log_index] + len, LOG_MSG_MAX_LEN - len, format, args);

    s_log_index = (s_log_index + 1) % CONFIG_LOG_BUFFER_SIZE;
    if (s_log_count < CONFIG_LOG_BUFFER_SIZE) {
        s_log_count++;
    }

    xSemaphoreGive(s_log_mutex);
    va_end(args);
}

// Handler for root page
static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, DEBUG_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Handler for status
static esp_err_t status_handler(httpd_req_t *req)
{
    wifi_manager_status_t wifi = wifi_manager_get_status();
    ota_progress_t ota = ota_handler_get_progress();

    int64_t uptime = (esp_timer_get_time() - s_boot_time) / 1000000;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "version", ota_handler_get_version());
    cJSON_AddNumberToObject(root, "uptime", uptime);
    cJSON_AddStringToObject(root, "ssid", wifi.ssid);
    cJSON_AddStringToObject(root, "ip", wifi.ip_addr);
    cJSON_AddNumberToObject(root, "rssi", wifi.rssi);
    cJSON_AddNumberToObject(root, "heap", esp_get_free_heap_size());

    // OTA status
    const char *ota_status_str;
    switch (ota.status) {
        case OTA_STATUS_DOWNLOADING: ota_status_str = "downloading"; break;
        case OTA_STATUS_VERIFYING: ota_status_str = "verifying"; break;
        case OTA_STATUS_SUCCESS: ota_status_str = "success"; break;
        case OTA_STATUS_FAILED: ota_status_str = "failed"; break;
        default: ota_status_str = "idle"; break;
    }
    cJSON_AddStringToObject(root, "ota_status", ota_status_str);
    cJSON_AddNumberToObject(root, "ota_progress", ota.progress);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

// Handler for logs
static esp_err_t logs_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *logs = cJSON_CreateArray();

    xSemaphoreTake(s_log_mutex, portMAX_DELAY);

    int start = (s_log_count < CONFIG_LOG_BUFFER_SIZE) ? 0 :
                (s_log_index);
    for (int i = 0; i < s_log_count; i++) {
        int idx = (start + i) % CONFIG_LOG_BUFFER_SIZE;
        cJSON_AddItemToArray(logs, cJSON_CreateString(s_log_buffer[idx]));
    }

    xSemaphoreGive(s_log_mutex);

    cJSON_AddItemToObject(root, "logs", logs);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

// Handler for OTA
static esp_err_t ota_handler(httpd_req_t *req)
{
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *url_json = cJSON_GetObjectItem(root, "url");
    if (!cJSON_IsString(url_json)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "URL required");
        return ESP_FAIL;
    }

    esp_err_t err = ota_handler_start(url_json->valuestring);
    cJSON_Delete(root);

    cJSON *response = cJSON_CreateObject();
    if (err == ESP_OK) {
        cJSON_AddBoolToObject(response, "success", true);
        cJSON_AddStringToObject(response, "message", "OTA started");
        debug_server_log("OTA started: %s", url_json->valuestring);
    } else {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", esp_err_to_name(err));
    }

    char *json = cJSON_PrintUnformatted(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);
    cJSON_Delete(response);
    return ESP_OK;
}

// Handler for type test (placeholder - HID disabled in Phase 1)
static esp_err_t type_handler(httpd_req_t *req)
{
    cJSON *response = cJSON_CreateObject();

#if CONFIG_ENABLE_HID
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "message", "Typed 'hello world'");
    debug_server_log("Keyboard output triggered");
#else
    cJSON_AddBoolToObject(response, "success", false);
    cJSON_AddStringToObject(response, "message", "HID disabled in Phase 1");
    debug_server_log("Type requested but HID disabled");
#endif

    char *json = cJSON_PrintUnformatted(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);
    cJSON_Delete(response);
    return ESP_OK;
}

// Handler for reset WiFi
static esp_err_t reset_wifi_handler(httpd_req_t *req)
{
    debug_server_log("WiFi reset requested");
    wifi_manager_clear_credentials();

    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "message", "WiFi credentials cleared, rebooting...");

    char *json = cJSON_PrintUnformatted(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);
    cJSON_Delete(response);

    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

    return ESP_OK;
}

// Handler for reboot
static esp_err_t reboot_handler(httpd_req_t *req)
{
    debug_server_log("Reboot requested");

    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "message", "Rebooting...");

    char *json = cJSON_PrintUnformatted(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);
    cJSON_Delete(response);

    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

    return ESP_OK;
}

esp_err_t debug_server_start(void)
{
    if (s_server != NULL) {
        ESP_LOGW(TAG, "Server already running");
        return ESP_OK;
    }

    // Initialize log mutex
    if (s_log_mutex == NULL) {
        s_log_mutex = xSemaphoreCreateMutex();
    }

    // Record boot time
    if (s_boot_time == 0) {
        s_boot_time = esp_timer_get_time();
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 12;

    ESP_LOGI(TAG, "Starting debug server on port %d", config.server_port);

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start server: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register handlers
    httpd_uri_t handlers[] = {
        {.uri = "/", .method = HTTP_GET, .handler = root_handler},
        {.uri = "/status", .method = HTTP_GET, .handler = status_handler},
        {.uri = "/logs", .method = HTTP_GET, .handler = logs_handler},
        {.uri = "/ota", .method = HTTP_POST, .handler = ota_handler},
        {.uri = "/type", .method = HTTP_POST, .handler = type_handler},
        {.uri = "/reset-wifi", .method = HTTP_POST, .handler = reset_wifi_handler},
        {.uri = "/reboot", .method = HTTP_POST, .handler = reboot_handler},
    };

    for (int i = 0; i < sizeof(handlers) / sizeof(handlers[0]); i++) {
        httpd_register_uri_handler(s_server, &handlers[i]);
    }

    debug_server_log("Debug server started");
    ESP_LOGI(TAG, "Debug server started");
    return ESP_OK;
}

esp_err_t debug_server_stop(void)
{
    if (s_server == NULL) {
        return ESP_OK;
    }

    esp_err_t ret = httpd_stop(s_server);
    s_server = NULL;
    ESP_LOGI(TAG, "Debug server stopped");
    return ret;
}

bool debug_server_is_running(void)
{
    return s_server != NULL;
}
