#include "captive_portal.h"
#include "wifi_manager.h"
#include "config.h"

#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"

static const char *TAG = "captive";
static httpd_handle_t s_server = NULL;

// Embedded HTML for captive portal
static const char CAPTIVE_PORTAL_HTML[] =
"<!DOCTYPE html>"
"<html><head>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>IOS-Keyboard Setup</title>"
"<style>"
"body{font-family:Arial,sans-serif;margin:20px;background:#1a1a2e;color:#eee;}"
"h1{color:#0f0;text-align:center;}"
".container{max-width:400px;margin:0 auto;}"
".card{background:#16213e;padding:20px;border-radius:10px;margin:10px 0;}"
"input,select{width:100%;padding:12px;margin:8px 0;box-sizing:border-box;"
"border:1px solid #0f0;border-radius:5px;background:#0f3460;color:#eee;}"
"button{width:100%;padding:12px;background:#0f0;color:#000;border:none;"
"border-radius:5px;cursor:pointer;font-weight:bold;margin-top:10px;}"
"button:hover{background:#0c0;}"
".network{padding:10px;margin:5px 0;background:#0f3460;border-radius:5px;"
"cursor:pointer;display:flex;justify-content:space-between;}"
".network:hover{background:#1a4a7a;}"
".rssi{color:#0f0;}"
".status{text-align:center;padding:10px;margin:10px 0;border-radius:5px;}"
".success{background:#0f03;border:1px solid #0f0;}"
".error{background:#f003;border:1px solid #f00;}"
".loading{color:#ff0;}"
"</style>"
"</head><body>"
"<div class='container'>"
"<h1>IOS-Keyboard Setup</h1>"
"<div class='card'>"
"<h3>Available Networks</h3>"
"<div id='networks'><p class='loading'>Scanning...</p></div>"
"<button onclick='scan()'>Refresh</button>"
"</div>"
"<div class='card'>"
"<h3>Connect to Network</h3>"
"<form id='wifiForm'>"
"<input type='text' id='ssid' name='ssid' placeholder='Network Name (SSID)' required>"
"<input type='password' id='password' name='password' placeholder='Password'>"
"<button type='submit'>Connect</button>"
"</form>"
"<div id='status'></div>"
"</div>"
"</div>"
"<script>"
"function scan(){"
"document.getElementById('networks').innerHTML='<p class=\"loading\">Scanning...</p>';"
"fetch('/scan').then(r=>r.json()).then(data=>{"
"let html='';"
"if(data.networks&&data.networks.length>0){"
"data.networks.forEach(n=>{"
"html+='<div class=\"network\" onclick=\"selectNetwork(\\''+n.ssid+'\\')\"><span>'+n.ssid+'</span><span class=\"rssi\">'+n.rssi+' dBm</span></div>';"
"});}else{html='<p>No networks found</p>';}"
"document.getElementById('networks').innerHTML=html;"
"}).catch(e=>{document.getElementById('networks').innerHTML='<p class=\"error\">Scan failed</p>';});}"
"function selectNetwork(ssid){document.getElementById('ssid').value=ssid;}"
"document.getElementById('wifiForm').onsubmit=function(e){"
"e.preventDefault();"
"let ssid=document.getElementById('ssid').value;"
"let pass=document.getElementById('password').value;"
"document.getElementById('status').innerHTML='<p class=\"loading\">Connecting...</p>';"
"fetch('/connect',{method:'POST',headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({ssid:ssid,password:pass})}).then(r=>r.json()).then(data=>{"
"if(data.success){"
"document.getElementById('status').innerHTML='<p class=\"status success\">Connected! Device will restart...</p>';"
"}else{"
"document.getElementById('status').innerHTML='<p class=\"status error\">Failed: '+data.message+'</p>';"
"}}).catch(e=>{document.getElementById('status').innerHTML='<p class=\"status error\">Error: '+e+'</p>';});};"
"scan();"
"</script>"
"</body></html>";

// Handler for root page
static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, CAPTIVE_PORTAL_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Handler for network scan
static esp_err_t scan_handler(httpd_req_t *req)
{
    char ssids[20][33];
    int8_t rssi[20];
    int count = wifi_manager_scan(ssids, rssi, 20);

    cJSON *root = cJSON_CreateObject();
    cJSON *networks = cJSON_CreateArray();

    for (int i = 0; i < count; i++) {
        cJSON *network = cJSON_CreateObject();
        cJSON_AddStringToObject(network, "ssid", ssids[i]);
        cJSON_AddNumberToObject(network, "rssi", rssi[i]);
        cJSON_AddItemToArray(networks, network);
    }

    cJSON_AddItemToObject(root, "networks", networks);
    char *json = cJSON_PrintUnformatted(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

// Handler for connect request
static esp_err_t connect_handler(httpd_req_t *req)
{
    char buf[256];
    int ret, remaining = req->content_len;

    if (remaining > sizeof(buf) - 1) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *ssid_json = cJSON_GetObjectItem(root, "ssid");
    cJSON *pass_json = cJSON_GetObjectItem(root, "password");

    if (!cJSON_IsString(ssid_json) || strlen(ssid_json->valuestring) == 0) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID required");
        return ESP_FAIL;
    }

    const char *ssid = ssid_json->valuestring;
    const char *password = cJSON_IsString(pass_json) ? pass_json->valuestring : "";

    ESP_LOGI(TAG, "Attempting connection to: %s", ssid);

    // Try to connect
    esp_err_t err = wifi_manager_try_connect(ssid, password);

    cJSON *response = cJSON_CreateObject();
    if (err == ESP_OK) {
        // Save credentials and schedule restart
        wifi_manager_save_credentials(ssid, password);
        cJSON_AddBoolToObject(response, "success", true);
        cJSON_AddStringToObject(response, "message", "Connected successfully");
    } else {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "Connection failed");
        // Restart AP mode
        wifi_manager_start_ap();
    }

    char *json = cJSON_PrintUnformatted(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);
    cJSON_Delete(response);
    cJSON_Delete(root);

    // If connection successful, restart after short delay
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Credentials saved, restarting in 2 seconds...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    }

    return ESP_OK;
}

// Handler for status request
static esp_err_t status_handler(httpd_req_t *req)
{
    wifi_manager_status_t status = wifi_manager_get_status();

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "mode", status.mode == WIFI_MGR_MODE_AP ? "ap" : "sta");
    cJSON_AddBoolToObject(root, "connected", status.connected);
    cJSON_AddStringToObject(root, "ssid", status.ssid);
    cJSON_AddStringToObject(root, "ip", status.ip_addr);
    cJSON_AddNumberToObject(root, "rssi", status.rssi);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

// Captive portal redirect handler (for any unknown URL)
static esp_err_t redirect_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

esp_err_t captive_portal_start(void)
{
    if (s_server != NULL) {
        ESP_LOGW(TAG, "Server already running");
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 10;

    ESP_LOGI(TAG, "Starting captive portal on port %d", config.server_port);

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start server: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register URI handlers
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
    };
    httpd_register_uri_handler(s_server, &root_uri);

    httpd_uri_t scan_uri = {
        .uri = "/scan",
        .method = HTTP_GET,
        .handler = scan_handler,
    };
    httpd_register_uri_handler(s_server, &scan_uri);

    httpd_uri_t connect_uri = {
        .uri = "/connect",
        .method = HTTP_POST,
        .handler = connect_handler,
    };
    httpd_register_uri_handler(s_server, &connect_uri);

    httpd_uri_t status_uri = {
        .uri = "/status",
        .method = HTTP_GET,
        .handler = status_handler,
    };
    httpd_register_uri_handler(s_server, &status_uri);

    // Wildcard handler for captive portal redirect
    httpd_uri_t redirect_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = redirect_handler,
    };
    httpd_register_uri_handler(s_server, &redirect_uri);

    ESP_LOGI(TAG, "Captive portal started");
    return ESP_OK;
}

esp_err_t captive_portal_stop(void)
{
    if (s_server == NULL) {
        return ESP_OK;
    }

    esp_err_t ret = httpd_stop(s_server);
    s_server = NULL;
    ESP_LOGI(TAG, "Captive portal stopped");
    return ret;
}

bool captive_portal_is_running(void)
{
    return s_server != NULL;
}
