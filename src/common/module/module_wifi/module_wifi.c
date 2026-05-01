// MODULE_WIFI
// SEPTEMBER 6, 2025

#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_http_server.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include "module_wifi.h"
#include "define_common_data_types.h"
#include "define_rtos_tasks.h"
#include "project_defines.h"

// NVS
#define NVS_NAMESPACE               "bambu_cfg"
#define CAPTIVE_PORTAL_HTML_BUF_SIZE (4096)
#define NVS_KEY_SSID        "ssid"
#define NVS_KEY_PWD         "pwd"
#define NVS_KEY_API_KEY     "api_key"
#define NVS_KEY_USER_ID     "user_id"
#define NVS_KEY_DEVICE_ID   "device_id"
#define NVS_KEY_REGION      "region"

// Extern Variables
TaskHandle_t handle_task_module_wifi;

// Local Variables
static module_wifi_state_t s_state;
static module_wifi_state_t s_state_prev;
static util_dataqueue_t s_dataqueue;
static uint8_t s_notification_targets_count;
static util_dataqueue_t* s_notification_targets[MODULE_WIFI_NOTIFICATION_TARGET_MAX];
static util_dataqueue_item_t s_dq_i;
static module_wifi_state_t s_wifi_credential_source;
static rtos_component_type_t s_component_type;
static esp_timer_handle_t s_wifi_timer_handle;
static esp_timer_handle_t s_ap_timer_handle;
static uint8_t s_wifi_retry_count;
static TaskHandle_t s_dns_task_handle;
static httpd_handle_t s_http_server;
static volatile bool s_portal_connect_pending;
static bool s_ap_window_active;
static bool s_ap_window_expired;
static uint8_t s_ap_client_count;
static char* s_html_buf;

// Local Functions
static bool s_notify(util_dataqueue_item_t* dq_i, TickType_t wait);
static void s_state_set(module_wifi_state_t newstate);
static void s_state_mainiter(void);
static void s_nvs_read_str(const char* key, char* out, size_t out_len);
static void s_nvs_write_str(const char* key, const char* val);
static void s_url_decode(const char* in, char* out, size_t out_len);
static void s_form_field(const char* body, const char* key, char* out, size_t out_len);
static void s_captive_portal_start(void);
static void s_captive_portal_stop(void);
// Callbacks
static void s_task_function(void *pvParameters);
static void s_timer_cb(void *arg);
static void s_ap_timer_cb(void *arg);
static void s_dns_task(void *arg);
static esp_err_t s_http_get_handler(httpd_req_t *req);
static esp_err_t s_http_post_handler(httpd_req_t *req);
static esp_err_t s_http_redirect_handler(httpd_req_t *req);

// External Functions
bool MODULE_WIFI_Init(void)
{
    // Initialize Module Wifi

    s_component_type = COMPONENT_TYPE_TASK;
    s_state = -1;
    s_state_prev = -1;
    s_state_set(MODULE_WIFI_STATE_IDLE);

    s_http_server = NULL;
    s_dns_task_handle = NULL;
    s_portal_connect_pending = false;
    s_ap_window_active = false;
    s_ap_window_expired = false;
    s_ap_client_count = 0;
    s_html_buf = NULL;

    // Create Data Queue
    UTIL_DATAQUEUE_Create(&s_dataqueue, MODULE_WIFI_DATAQUEUE_MAX);
    s_notification_targets_count = 0;

    // Create Task
    xTaskCreate(
        s_task_function,
        "t-m-wifi",
        TASK_STACK_DEPTH_MODULE_WIFI,
        NULL,
        TASK_PRIORITY_MODULE_WIFI,
        &handle_task_module_wifi
    );

    // Setup Wifi Connect Timer
    const esp_timer_create_args_t timer_args = {
        .callback = &s_timer_cb,
        .arg = (void*)0,     // optional user data
        .name = "one-shot"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_wifi_timer_handle));

    // Setup AP Window Timer
    const esp_timer_create_args_t ap_timer_args = {
        .callback = &s_ap_timer_cb,
        .arg = (void*)0,
        .name = "ap-window"
    };
    ESP_ERROR_CHECK(esp_timer_create(&ap_timer_args, &s_ap_timer_handle));

    // Add Notification Targets
    DRIVER_WIFI_AddNotificationTarget(&s_dataqueue);

    ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "Type %u. Init", s_component_type);

    return true;
}

bool MODULE_WIFI_AddCommand(util_dataqueue_item_t* dq_i)
{
    // Add Command

    if(!UTIL_DATAQUEUE_MessageQueue(&s_dataqueue, dq_i, 0)){
        ESP_LOGW(DEBUG_TAG_MODULE_WIFI, "Message Queue Failed %s", __FILE__);

        return false;
    }

    return true;
}

bool MODULE_WIFI_AddNotificationTarget(util_dataqueue_t* dq)
{
    // Add Notification Target

    if(s_notification_targets_count >= MODULE_WIFI_NOTIFICATION_TARGET_MAX){
        return false;
    }

    s_notification_targets[s_notification_targets_count] = dq;
    s_notification_targets_count += 1;

    return true;
}

static bool s_notify(util_dataqueue_item_t* dq_i, TickType_t wait)
{
    // Send Notification

    for(uint8_t i = 0; i < s_notification_targets_count; i++){
        if(!UTIL_DATAQUEUE_MessageQueue(s_notification_targets[i], dq_i, wait)){
            ESP_LOGW(DEBUG_TAG_MODULE_WIFI, "Message Queue Failed %s", __FILE__);
        }
    }

    return true;
}

static void s_state_set(module_wifi_state_t newstate)
{
    // Module Wifi Set State

    if(s_state == newstate){
        return;
    }

    s_state_prev = s_state;
    s_state = newstate;

    ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "%u -> %u", s_state_prev, s_state);
}

static void s_state_mainiter(void)
{
    // State Mainiter

    util_dataqueue_item_t dq_i;

    switch(s_state)
    {
        case MODULE_WIFI_STATE_IDLE:
            // On Initial Boot (s_state_prev == -1), Activate AP Window
            if(s_state_prev == (module_wifi_state_t)-1){
                s_state_set(MODULE_WIFI_STATE_AP);
            }
            break;

        case MODULE_WIFI_STATE_AP:
            if(s_state_prev != s_state){
                dq_i.data_type = DATA_TYPE_COMMAND;
                dq_i.data = DRIVER_WIFI_COMMAND_SCAN;
                DRIVER_WIFI_AddCommand(&dq_i);
                s_state_prev = s_state;
            }
            break;

        case MODULE_WIFI_STATE_CHECK_SAVED_CREDENTIALS:
            s_wifi_retry_count = 0;
            dq_i.data_type = DATA_TYPE_NOTIFICATION;
            dq_i.data = MODULE_WIFI_NOTIFICATION_CHECKING_SAVED_CREDENTIALS;
            s_notify(&dq_i, 0);

            if(DRIVER_WIFI_CheckSavedWifiCredentials())
            {
                s_wifi_credential_source = MODULE_WIFI_STATE_CHECK_SAVED_CREDENTIALS;
                s_wifi_retry_count = MODULE_WIFI_WIFI_CONNECT_RETRY_MAX;
                s_state_set(MODULE_WIFI_STATE_CONNECT);
                break;
            }

            ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "No Saved Wi-Fi Credential Found");
            s_state_set(MODULE_WIFI_STATE_CHECK_DEFAULT_CREDENTIALS);
            break;

        case MODULE_WIFI_STATE_CHECK_DEFAULT_CREDENTIALS:
            s_wifi_retry_count = 0;
            dq_i.data_type = DATA_TYPE_NOTIFICATION;
            dq_i.data = MODULE_WIFI_NOTIFICATION_CHECKING_DEFAULT_CREDENTIALS;
            s_notify(&dq_i, 0);
            
            #if defined(DEFAULT_WIFI_SSID) && defined(DEFAULT_WIFI_PASSWORD)
                    ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "Default Wi-Fi Credential Found");
                    ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "   SSID: %s", DEFAULT_WIFI_SSID);
                    ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "   Password: %s", DEFAULT_WIFI_PASSWORD);

                    // Set Wifi Credentials
                    DRIVER_WIFI_SetWifiCredentials((uint8_t*)DEFAULT_WIFI_SSID, (uint8_t*)DEFAULT_WIFI_PASSWORD);

                    // WiFi Connect
                    s_wifi_credential_source = MODULE_WIFI_STATE_CHECK_DEFAULT_CREDENTIALS;
                    s_wifi_retry_count = MODULE_WIFI_WIFI_CONNECT_RETRY_MAX;
                    s_state_set(MODULE_WIFI_STATE_CONNECT);
                    break;
            #endif

            // No Credentials Found - Retry
            ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "No Default Wi-Fi Credential Found");
            s_state_set(MODULE_WIFI_STATE_CHECK_SAVED_CREDENTIALS);
            break;

        case MODULE_WIFI_STATE_CONNECT:
            dq_i.data_type = DATA_TYPE_COMMAND;
            dq_i.data = DRIVER_WIFI_COMMAND_CONNECT;
            DRIVER_WIFI_AddCommand(&dq_i);

            // Start Timer
            ESP_ERROR_CHECK(esp_timer_start_once(s_wifi_timer_handle, MODULE_WIFI_WIFI_CONNECT_TIMEOUT_SEC * 1000000));
            s_state_set(MODULE_WIFI_STATE_CONNECTING);
            break;

        case MODULE_WIFI_STATE_CONNECTING:
            // Do Nothing
            break;

        case MODULE_WIFI_STATE_CONNECTED:
            if(s_state_prev != s_state){
                dq_i.data_type = DATA_TYPE_NOTIFICATION;
                dq_i.data = MODULE_WIFI_NOTIFICATION_CONNECTED;
                s_notify(&dq_i, 0);
                s_state_prev = s_state;
            }
            break;

        case MODULE_WIFI_STATE_GOT_IP:
            // Stop Timer
            dq_i.data_type = DATA_TYPE_NOTIFICATION;
            dq_i.data = MODULE_WIFI_NOTIFICATION_GOT_IP;
            memcpy(dq_i.data_buff.value.ip, s_dq_i.data_buff.value.ip, sizeof(dq_i.data_buff.value.ip));
            s_notify(&dq_i, 0);
            
            if(esp_timer_is_active(s_wifi_timer_handle)){
                ESP_ERROR_CHECK(esp_timer_stop(s_wifi_timer_handle));
            }
            s_state_set(MODULE_WIFI_STATE_IDLE);

            break;

        case MODULE_WIFI_STATE_LOST_IP:
        case MODULE_WIFI_STATE_DISCONNECTED:
            // Lost IP
            // Restart Connection Logic
            // Start New Connection Attempt
            if(esp_timer_is_active(s_wifi_timer_handle)){
                ESP_ERROR_CHECK(esp_timer_stop(s_wifi_timer_handle));
            }
            s_state_set(MODULE_WIFI_STATE_CHECK_SAVED_CREDENTIALS);
            break;

            default:
                break;
    }
}

static void s_task_function(void *pvParameters)
{
    // Task Function

    ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "Starting task");

    memset(&s_dq_i, 0, sizeof(util_dataqueue_item_t));
    while(true){
        // Check Data Queue
        if(UTIL_DATAQUEUE_MessageCheck(&s_dataqueue))
        {
            if(UTIL_DATAQUEUE_MessageGet(&s_dataqueue, &s_dq_i, 0))
            {
                ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "New In DataQueue. Type %u, Data %u", s_dq_i.data_type, s_dq_i.data);

                if(s_dq_i.data_type == DATA_TYPE_COMMAND)
                {
                    switch(s_dq_i.data)
                    {
                        case MODULE_WIFI_COMMAND_CONNECT:
                            s_state_set(MODULE_WIFI_STATE_CHECK_SAVED_CREDENTIALS);
                            break;

                        default:
                            break;
                    }
                }
                else if(s_dq_i.data_type == DATA_TYPE_NOTIFICATION)
                {
                    // Take Action On Notification
                    switch(s_dq_i.data)
                    {
                        case DRIVER_WIFI_NOTIFICATION_SCAN_DONE:
                            ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "Scan done");
                            if(s_state == MODULE_WIFI_STATE_AP){
                                s_dq_i.data_type = DATA_TYPE_COMMAND;
                                s_dq_i.data = DRIVER_WIFI_COMMAND_AP_BROADCAST;
                                DRIVER_WIFI_AddCommand(&s_dq_i);
                            }
                            break;

                        case DRIVER_WIFI_NOTIFICATION_CONNECTED:
                            s_state_set(MODULE_WIFI_STATE_CONNECTED);
                            break;

                        case DRIVER_WIFI_NOTIFICATION_GOT_IP:
                            s_state_set(MODULE_WIFI_STATE_GOT_IP);
                            break;

                        case DRIVER_WIFI_NOTIFICATION_LOST_IP:
                            // Ignore If Not In Idle State
                            if(s_state == MODULE_WIFI_STATE_IDLE){
                                s_state_set(MODULE_WIFI_STATE_LOST_IP);
                            }
                            break;

                        case DRIVER_WIFI_NOTIFICATION_DISCONNECTED:
                            // Ignore If Not In Idle State
                            if(s_state == MODULE_WIFI_STATE_IDLE){
                                s_state_set(MODULE_WIFI_STATE_DISCONNECTED);
                            }
                            break;

                        case DRIVER_WIFI_NOTIFICATION_AP_STA_CONNECTED:
                            s_ap_client_count += 1;
                            ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "AP client connected (%u total)", s_ap_client_count);
                            break;

                        case DRIVER_WIFI_NOTIFICATION_AP_STA_DISCONNECTED:
                            if(s_ap_client_count > 0) s_ap_client_count -= 1;
                            ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "AP client disconnected (%u remaining)", s_ap_client_count);
                            if(s_ap_window_expired && s_ap_client_count == 0){
                                s_ap_window_expired = false;
                                s_ap_window_active = false;
                                s_captive_portal_stop();
                                s_state_set(MODULE_WIFI_STATE_CHECK_SAVED_CREDENTIALS);
                                ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "Last AP client disconnected, resuming wifi cycle");
                            }
                            break;

                        case DRIVER_WIFI_NOTIFICATION_APSTARTED:
                            s_captive_portal_start();
                            s_ap_window_active = true;
                            ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "AP window: %us", MODULE_WIFI_AP_WINDOW_SEC);
                            ESP_ERROR_CHECK(esp_timer_start_once(
                                s_ap_timer_handle,
                                (uint64_t)MODULE_WIFI_AP_WINDOW_SEC * 1000000
                            ));
                            s_dq_i.data_type = DATA_TYPE_NOTIFICATION;
                            s_dq_i.data = MODULE_WIFI_NOTIFICATION_APSTARTED;
                            s_notify(&s_dq_i, 0);
                            break;

                        default:
                            break;
                    }
                }
            }
        }

        // Handle Portal Connect Request
        if(s_portal_connect_pending) {
            s_portal_connect_pending = false;
            if(esp_timer_is_active(s_ap_timer_handle)){
                ESP_ERROR_CHECK(esp_timer_stop(s_ap_timer_handle));
            }
            s_ap_window_active = false;
            s_captive_portal_stop();
            if(esp_timer_is_active(s_wifi_timer_handle)){
                ESP_ERROR_CHECK(esp_timer_stop(s_wifi_timer_handle));
            }
            s_wifi_credential_source = MODULE_WIFI_STATE_CHECK_SAVED_CREDENTIALS;
            s_wifi_retry_count = MODULE_WIFI_WIFI_CONNECT_RETRY_MAX;
            s_state_set(MODULE_WIFI_STATE_CONNECT);
        }

        // Run State Mainiter
        s_state_mainiter();

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    vTaskDelete(NULL);
}

static void s_timer_cb(void *arg)
{
    // Timer Callback

    if(s_wifi_retry_count <= MODULE_WIFI_WIFI_CONNECT_RETRY_MAX){
        s_wifi_retry_count += 1;
        s_state_set(MODULE_WIFI_STATE_CONNECT);
        return;
    }

    // Retries Exhausted
    // Resume Wifi Credentials State Machine
    ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "%u Connect Retries Exhausted", MODULE_WIFI_WIFI_CONNECT_RETRY_MAX);
    switch(s_wifi_credential_source)
    {
        case MODULE_WIFI_STATE_CHECK_SAVED_CREDENTIALS:
            s_state_set(MODULE_WIFI_STATE_CHECK_DEFAULT_CREDENTIALS);
            break;

        case MODULE_WIFI_STATE_CHECK_DEFAULT_CREDENTIALS:
            s_state_set(MODULE_WIFI_STATE_CHECK_SAVED_CREDENTIALS);
            break;

        default:
            s_state_set(MODULE_WIFI_STATE_CHECK_SAVED_CREDENTIALS);
            break;
    }
}

static void s_dns_task(void *arg)
{
    // DNS Server Task
    // Responds to all DNS queries with AP IP 192.168.4.1 to redirect clients to captive portal

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sock < 0) {
        ESP_LOGE(DEBUG_TAG_MODULE_WIFI, "DNS socket failed");
        vTaskDelete(NULL);
        return;
    }

    // Non-blocking recv so this task can be deleted cleanly
    struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(53),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    if(bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(DEBUG_TAG_MODULE_WIFI, "DNS bind failed");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "DNS server running on port 53");

    uint8_t buf[256];
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);

    while(true) {
        int len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&client, &client_len);
        if(len < 12) continue;

        // Patch header: QR=1 (response), AA=1 (authoritative), RA=1, preserve RD
        buf[2] = 0x84 | (buf[2] & 0x01);
        buf[3] = 0x80; // RA=1; clients reject responses with RA=0
        // NSCOUNT = 0, ARCOUNT = 0
        buf[8] = 0x00; buf[9] = 0x00;
        buf[10] = 0x00; buf[11] = 0x00;

        // Find end of QNAME (skip labels until null terminator)
        int qname_end = 12;
        while(qname_end < len && buf[qname_end] != 0) qname_end += buf[qname_end] + 1;

        // Read QTYPE (2 bytes after null terminator)
        uint16_t qtype = ((uint16_t)buf[qname_end + 1] << 8) | buf[qname_end + 2];

        // End of question section: null(1) + QTYPE(2) + QCLASS(2)
        int pos = qname_end + 5;

        if(qtype == 0x0001 && pos + 16 <= (int)sizeof(buf)) {
            // TYPE A query: respond with A record for 192.168.4.1
            buf[6] = 0x00; buf[7] = 0x01; // ANCOUNT = 1
            buf[pos++] = 0xC0; buf[pos++] = 0x0C; // name pointer to offset 12
            buf[pos++] = 0x00; buf[pos++] = 0x01; // TYPE A
            buf[pos++] = 0x00; buf[pos++] = 0x01; // CLASS IN
            buf[pos++] = 0x00; buf[pos++] = 0x00; buf[pos++] = 0x00; buf[pos++] = 0x3C; // TTL 60
            buf[pos++] = 0x00; buf[pos++] = 0x04; // RDLENGTH
            buf[pos++] = 192;  buf[pos++] = 168;  buf[pos++] = 4;   buf[pos++] = 1;
        } else {
            // AAAA or other types: NOERROR with 0 answers; client will retry with A
            buf[6] = 0x00; buf[7] = 0x00; // ANCOUNT = 0
        }

        sendto(sock, buf, pos, 0, (struct sockaddr*)&client, client_len);
    }

    close(sock);
    vTaskDelete(NULL);
}

static esp_err_t s_http_get_handler(httpd_req_t *req)
{
    // Serve Captive Portal Configuration Form

    wifi_ap_record_t* records;
    uint16_t count;
    DRIVER_WIFI_GetScanResults(&records, &count);

    char saved_ssid[DRIVER_WIFI_LEN_SSID_MAX] = {0};
    char saved_pwd[DRIVER_WIFI_LEN_PWD_MAX]   = {0};
    char saved_api_key[256] = {0};
    char saved_user_id[64] = {0};
    char saved_device_id[64] = {0};
    char saved_region[16] = {0};
    s_nvs_read_str(NVS_KEY_SSID,      saved_ssid,      sizeof(saved_ssid));
    s_nvs_read_str(NVS_KEY_PWD,       saved_pwd,       sizeof(saved_pwd));
    s_nvs_read_str(NVS_KEY_API_KEY,   saved_api_key,   sizeof(saved_api_key));
    s_nvs_read_str(NVS_KEY_USER_ID,   saved_user_id,   sizeof(saved_user_id));
    s_nvs_read_str(NVS_KEY_DEVICE_ID, saved_device_id, sizeof(saved_device_id));
    s_nvs_read_str(NVS_KEY_REGION,    saved_region,    sizeof(saved_region));

    // Build <option> tags from scan results
    char options[512] = {0};
    size_t opt_len = 0;
    for(uint16_t i = 0; i < count && opt_len < sizeof(options) - 80; i++) {
        const char* ssid = (const char*)records[i].ssid;
        if(ssid[0] == '\0') continue;
        bool selected = strcmp(ssid, saved_ssid) == 0;
        opt_len += snprintf(options + opt_len, sizeof(options) - opt_len,
            "<option value=\"%s\"%s>%s (%d dBm)</option>",
            ssid, selected ? " selected" : "", ssid, records[i].rssi);
    }

    // Region selected attributes
    bool region_china = strcmp(saved_region, "china") == 0;
    const char* sel_global = region_china ? "" : " selected";
    const char* sel_china  = region_china ? " selected" : "";

    snprintf(s_html_buf, CAPTIVE_PORTAL_HTML_BUF_SIZE,
        "<!DOCTYPE html><html><head>"
        "<meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Bambu Watcher Setup</title>"
        "<style>"
        "body{font-family:sans-serif;max-width:420px;margin:40px auto;padding:0 16px}"
        "h2{color:#1565C0}"
        "label{display:block;margin-top:14px;font-weight:600;font-size:14px}"
        "select,input{width:100%%;padding:8px;box-sizing:border-box;margin-top:4px;"
                     "border:1px solid #ccc;border-radius:4px}"
        "button{margin-top:24px;width:100%%;padding:12px;background:#1976D2;color:#fff;"
               "border:none;border-radius:4px;font-size:16px;cursor:pointer}"
        "button:hover{background:#1565C0}"
        ".err{color:#c62828;font-size:13px;margin-top:4px;display:none}"
        "</style>"
        "<script>"
        "function v(){"
          "var ok=true;"
          "['ssid','api_key','user_id','device_id'].forEach(function(n){"
            "var el=document.getElementsByName(n)[0];"
            "var e=el.parentNode.querySelector('.err');"
            "if(!el.value.trim()){e.style.display='block';ok=false;}"
            "else{e.style.display='none';}"
          "});"
          "return ok;"
        "}"
        "</script>"
        "</head><body>"
        "<h2>Bambu Watcher Setup</h2>"
        "<form method='POST' action='/save' onsubmit='return v()'>"
        "<label>Wi-Fi Network</label>"
        "<select name='ssid'>%s</select>"
        "<span class='err'>Please select a Wi-Fi network</span>"
        "<label>Password</label>"
        "<input type='text' name='pwd' value=\"%s\">"
        "<label>Bambu API Key</label>"
        "<input type='text' name='api_key' value='%s' placeholder='Enter API key'>"
        "<span class='err'>API key is required</span>"
        "<label>Bambu User ID (Without Leading _u)</label>"
        "<input type='text' name='user_id' value='%s' placeholder='Enter user ID'>"
        "<span class='err'>User ID is required</span>"
        "<label>Bambu Device ID</label>"
        "<input type='text' name='device_id' value='%s' placeholder='Enter device ID'>"
        "<span class='err'>Device ID is required</span>"
        "<label>Bambu Region</label>"
        "<select name='region'>"
        "<option value='global'%s>Global</option>"
        "<option value='china'%s>China</option>"
        "</select>"
        "<button type='submit'>Save &amp; Connect</button>"
        "</form></body></html>",
        options, saved_pwd, saved_api_key, saved_user_id, saved_device_id, sel_global, sel_china);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, s_html_buf, strlen(s_html_buf));
    return ESP_OK;
}

static esp_err_t s_http_post_handler(httpd_req_t *req)
{
    // Handle Form Submission - Save Config To NVS And Initiate Connection

    char body[2048] = {0};
    int recv_len = req->content_len < (int)(sizeof(body) - 1) ? req->content_len : (int)(sizeof(body) - 1);
    if(recv_len > 0) httpd_req_recv(req, body, recv_len);
    body[recv_len] = '\0';

    char ssid[DRIVER_WIFI_LEN_SSID_MAX] = {0};
    char pwd[DRIVER_WIFI_LEN_PWD_MAX] = {0};
    char api_key[256] = {0};
    char user_id[64] = {0};
    char device_id[64] = {0};
    char region[16] = {0};

    s_form_field(body, "ssid", ssid, sizeof(ssid));
    s_form_field(body, "pwd", pwd, sizeof(pwd));
    s_form_field(body, "api_key", api_key, sizeof(api_key));
    s_form_field(body, "user_id", user_id, sizeof(user_id));
    s_form_field(body, "device_id", device_id, sizeof(device_id));
    s_form_field(body, "region", region, sizeof(region));

    ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "Portal save: SSID=%s region=%s", ssid, region);

    // Keep saved password if field was left blank
    if(pwd[0] == '\0') s_nvs_read_str(NVS_KEY_PWD, pwd, sizeof(pwd));

    // Persist To NVS
    s_nvs_write_str(NVS_KEY_SSID, ssid);
    s_nvs_write_str(NVS_KEY_PWD, pwd);
    s_nvs_write_str(NVS_KEY_API_KEY, api_key);
    s_nvs_write_str(NVS_KEY_USER_ID, user_id);
    s_nvs_write_str(NVS_KEY_DEVICE_ID, device_id);
    s_nvs_write_str(NVS_KEY_REGION, region);

    // Load Into Driver And Signal State Machine To Connect
    DRIVER_WIFI_SetWifiCredentials((uint8_t*)ssid, (uint8_t*)pwd);
    s_portal_connect_pending = true;

    const char* resp =
        "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Saved</title>"
        "<style>body{font-family:sans-serif;text-align:center;margin-top:80px}</style>"
        "</head><body>"
        "<h2>Saved! Connecting...</h2>"
        "<p>The device will now attempt to connect to Wi-Fi.</p>"
        "</body></html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static esp_err_t s_http_redirect_handler(httpd_req_t *req)
{
    // Redirect Captive Portal Detection URIs To Setup Page

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static void s_ap_timer_cb(void *arg)
{
    // AP Window Expired
    // If A Client Is Still Connected, Defer Teardown Until It Disconnects

    if(s_ap_client_count > 0){
        s_ap_window_expired = true;
        ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "AP window expired, deferring teardown - %u client(s) connected", s_ap_client_count);
    } else {
        s_ap_window_active = false;
        s_captive_portal_stop();
        s_state_set(MODULE_WIFI_STATE_CHECK_SAVED_CREDENTIALS);
        ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "AP window expired, resuming wifi cycle");
    }
}

static void s_captive_portal_start(void)
{
    // Start HTTP Server And DNS Hijack For Captive Portal

    if(s_http_server != NULL) return;

    s_html_buf = malloc(CAPTIVE_PORTAL_HTML_BUF_SIZE);
    if(!s_html_buf){
        ESP_LOGE(DEBUG_TAG_MODULE_WIFI, "Captive portal html buf malloc failed");
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.stack_size = 8192;

    if(httpd_start(&s_http_server, &config) != ESP_OK) {
        ESP_LOGE(DEBUG_TAG_MODULE_WIFI, "HTTP server start failed");
        return;
    }

    static const httpd_uri_t uri_root = {
        .uri = "/", .method = HTTP_GET, .handler = s_http_get_handler
    };
    static const httpd_uri_t uri_save = {
        .uri = "/save", .method = HTTP_POST, .handler = s_http_post_handler
    };
    static const httpd_uri_t uri_g204 = {
        .uri = "/generate_204", .method = HTTP_GET, .handler = s_http_redirect_handler
    };
    static const httpd_uri_t uri_hotspot = {
        .uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = s_http_redirect_handler
    };
    static const httpd_uri_t uri_ncsi = {
        .uri = "/ncsi.txt", .method = HTTP_GET, .handler = s_http_redirect_handler
    };
    static const httpd_uri_t uri_fwlink = {
        .uri = "/fwlink", .method = HTTP_GET, .handler = s_http_redirect_handler
    };

    httpd_register_uri_handler(s_http_server, &uri_root);
    httpd_register_uri_handler(s_http_server, &uri_save);
    httpd_register_uri_handler(s_http_server, &uri_g204);
    httpd_register_uri_handler(s_http_server, &uri_hotspot);
    httpd_register_uri_handler(s_http_server, &uri_ncsi);
    httpd_register_uri_handler(s_http_server, &uri_fwlink);

    xTaskCreate(
        s_dns_task,
        "t-dns",
        4096,
        NULL,
        TASK_PRIORITY_MODULE_WIFI,
        &s_dns_task_handle
    );

    ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "Captive portal started at http://192.168.4.1/");
}

static void s_captive_portal_stop(void)
{
    // Stop HTTP Server, DNS Task, And AP Interface

    if(s_dns_task_handle != NULL){
        vTaskDelete(s_dns_task_handle);
        s_dns_task_handle = NULL;
    }

    if(s_http_server != NULL){
        httpd_stop(s_http_server);
        s_http_server = NULL;
    }

    DRIVER_WIFI_FreeScanResults();

    if(s_html_buf != NULL){
        free(s_html_buf);
        s_html_buf = NULL;
    }

    esp_wifi_set_mode(WIFI_MODE_STA);

    s_ap_window_expired = false;
    s_ap_client_count = 0;

    ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "Captive portal stopped");
}

static void s_nvs_read_str(const char* key, char* out, size_t out_len)
{
    // Read String From NVS; leaves out[0]='\0' on miss or error

    nvs_handle_t h;
    out[0] = '\0';
    if(nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return;
    size_t len = out_len;
    if(nvs_get_str(h, key, out, &len) != ESP_OK) out[0] = '\0';
    nvs_close(h);
}

static void s_nvs_write_str(const char* key, const char* val)
{
    // Write String To NVS

    nvs_handle_t h;
    if(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, key, val);
    nvs_commit(h);
    nvs_close(h);
}

static void s_url_decode(const char* in, char* out, size_t out_len)
{
    // Decode URL-encoded String (application/x-www-form-urlencoded)

    size_t i = 0, j = 0;
    while(in[i] && j < out_len - 1) {
        if(in[i] == '%' && in[i+1] && in[i+2]) {
            char hex[3] = {in[i+1], in[i+2], '\0'};
            out[j++] = (char)strtol(hex, NULL, 16);
            i += 3;
        } else if(in[i] == '+') {
            out[j++] = ' ';
            i++;
        } else {
            out[j++] = in[i++];
        }
    }
    out[j] = '\0';
}

static void s_form_field(const char* body, const char* key, char* out, size_t out_len)
{
    // Extract And Decode A Field From URL-encoded Form Body

    char search[64];
    snprintf(search, sizeof(search), "%s=", key);
    const char* p = strstr(body, search);
    if(!p) { out[0] = '\0'; return; }
    p += strlen(search);

    char encoded[768] = {0};
    size_t i = 0;
    while(*p && *p != '&' && i < sizeof(encoded) - 1) encoded[i++] = *p++;
    encoded[i] = '\0';

    s_url_decode(encoded, out, out_len);
}
