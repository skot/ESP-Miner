#include "http_server.h"
#include "recovery_page.h"
#include "theme_api.h"  // Add theme API include
#include "cJSON.h"
#include "esp_chip_info.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_vfs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "global_state.h"
#include "nvs_config.h"
#include "vcore.h"
#include <fcntl.h>
#include <string.h>
#include <sys/param.h>

#include "dns_server.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_wifi.h"
#include "lwip/err.h"
#include "lwip/inet.h"
#include "lwip/lwip_napt.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <pthread.h>

static const char * TAG = "http_server";
static const char * CORS_TAG = "CORS";

static GlobalState * GLOBAL_STATE;
static httpd_handle_t server = NULL;
QueueHandle_t log_queue = NULL;

static int fd = -1;

#define REST_CHECK(a, str, goto_tag, ...)                                                                                          \
    do {                                                                                                                           \
        if (!(a)) {                                                                                                                \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__);                                                  \
            goto goto_tag;                                                                                                         \
        }                                                                                                                          \
    } while (0)

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (10240)
#define MESSAGE_QUEUE_SIZE (128)

typedef struct rest_server_context
{
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

static esp_err_t ip_in_private_range(uint32_t address) {
    uint32_t ip_address = ntohl(address);

    // 10.0.0.0 - 10.255.255.255 (Class A)
    if ((ip_address >= 0x0A000000) && (ip_address <= 0x0AFFFFFF)) {
        return ESP_OK;
    }

    // 172.16.0.0 - 172.31.255.255 (Class B)
    if ((ip_address >= 0xAC100000) && (ip_address <= 0xAC1FFFFF)) {
        return ESP_OK;
    }

    // 192.168.0.0 - 192.168.255.255 (Class C)
    if ((ip_address >= 0xC0A80000) && (ip_address <= 0xC0A8FFFF)) {
        return ESP_OK;
    }

    return ESP_FAIL;
}

static uint32_t extract_origin_ip_addr(httpd_req_t *req)
{
    char origin[128];
    char ip_str[16];
    uint32_t origin_ip_addr = 0;

    // Attempt to get the Origin header.
    if (httpd_req_get_hdr_value_str(req, "Origin", origin, sizeof(origin)) != ESP_OK) {
        ESP_LOGD(CORS_TAG, "No origin header found.");
        return 0;
    }
    ESP_LOGD(CORS_TAG, "Origin header: %s", origin);

    // Find the start of the IP address in the Origin header
    const char *prefix = "http://";
    char *ip_start = strstr(origin, prefix);
    if (ip_start) {
        ip_start += strlen(prefix); // Move past "http://"

        // Extract the IP address portion (up to the next '/')
        char *ip_end = strchr(ip_start, '/');
        size_t ip_len = ip_end ? (size_t)(ip_end - ip_start) : strlen(ip_start);
        if (ip_len < sizeof(ip_str)) {
            strncpy(ip_str, ip_start, ip_len);
            ip_str[ip_len] = '\0'; // Null-terminate the string

            // Convert the IP address string to uint32_t
            origin_ip_addr = inet_addr(ip_str);
            if (origin_ip_addr == INADDR_NONE) {
                ESP_LOGW(CORS_TAG, "Invalid IP address: %s", ip_str);
            } else {
                ESP_LOGD(CORS_TAG, "Extracted IP address %lu", origin_ip_addr);
            }
        } else {
            ESP_LOGW(CORS_TAG, "IP address string is too long: %s", ip_start);
        }
    }

    return origin_ip_addr;
}

static esp_err_t is_network_allowed(httpd_req_t * req)
{
    if (GLOBAL_STATE->SYSTEM_MODULE.ap_enabled == true) {
        ESP_LOGI(CORS_TAG, "Device in AP mode. Allowing CORS.");
        return ESP_OK;
    }

    int sockfd = httpd_req_to_sockfd(req);
    char ipstr[INET6_ADDRSTRLEN];
    struct sockaddr_in6 addr;   // esp_http_server uses IPv6 addressing
    socklen_t addr_size = sizeof(addr);

    if (getpeername(sockfd, (struct sockaddr *)&addr, &addr_size) < 0) {
        ESP_LOGE(CORS_TAG, "Error getting client IP");
        return ESP_FAIL;
    }

    uint32_t request_ip_addr = addr.sin6_addr.un.u32_addr[3];

    // // Convert to IPv6 string
    // inet_ntop(AF_INET, &addr.sin6_addr, ipstr, sizeof(ipstr));

    // Convert to IPv4 string
    inet_ntop(AF_INET, &request_ip_addr, ipstr, sizeof(ipstr));

    uint32_t origin_ip_addr = extract_origin_ip_addr(req);
    if (origin_ip_addr == 0) {
        origin_ip_addr = request_ip_addr;
    }

    if (ip_in_private_range(origin_ip_addr) == ESP_OK && ip_in_private_range(request_ip_addr) == ESP_OK) {
        return ESP_OK;
    }

    ESP_LOGI(CORS_TAG, "Client is NOT in the private ip ranges or same range as server.");
    return ESP_FAIL;
}

esp_err_t init_fs(void)
{
    esp_vfs_spiffs_conf_t conf = {.base_path = "", .partition_label = NULL, .max_files = 5, .format_if_mount_failed = false};
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}

/* Function for stopping the webserver */
void stop_webserver(httpd_handle_t server)
{
    if (server) {
        /* Stop the httpd server */
        httpd_stop(server);
    }
}

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t * req, const char * filepath)
{
    const char * type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html")) {
        type = "text/html";
    } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
        type = "application/javascript";
    } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
        type = "text/css";
    } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
        type = "image/png";
    } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
        type = "text/xml";
    }
    return httpd_resp_set_type(req, type);
}

static esp_err_t set_cors_headers(httpd_req_t * req)
{

    esp_err_t err;

    err = httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    if (err != ESP_OK) {
        return ESP_FAIL;
    }

    err = httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
    if (err != ESP_OK) {
        return ESP_FAIL;
    }

    err = httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    if (err != ESP_OK) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

/* Recovery handler */
static esp_err_t rest_recovery_handler(httpd_req_t * req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    httpd_resp_send(req, recovery_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* Send HTTP response with the contents of the requested file */
static esp_err_t rest_common_get_handler(httpd_req_t * req)
{
    char filepath[FILE_PATH_MAX];
    uint8_t filePathLength = sizeof(filepath);

    rest_server_context_t * rest_context = (rest_server_context_t *) req->user_ctx;
    strlcpy(filepath, rest_context->base_path, filePathLength);
    if (req->uri[strlen(req->uri) - 1] == '/') {
        strlcat(filepath, "/index.html", filePathLength);
    } else {
        strlcat(filepath, req->uri, filePathLength);
    }
    set_content_type_from_file(req, filepath);
    strcat(filepath, ".gz");
    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        // Set status
        httpd_resp_set_status(req, "302 Temporary Redirect");
        // Redirect to the "/" root directory
        httpd_resp_set_hdr(req, "Location", "/");
        // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
        httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

        ESP_LOGI(TAG, "Redirecting to root");
        return ESP_OK;
    }

    if (req->uri[strlen(req->uri) - 1] != '/') {
        httpd_resp_set_hdr(req, "Cache-Control", "max-age=2592000");
    }

    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

    char * chunk = rest_context->scratch;
    ssize_t read_bytes;
    do {
        /* Read file in chunks into the scratch buffer */
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes == -1) {
            ESP_LOGE(TAG, "Failed to read file : %s", filepath);
        } else if (read_bytes > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                close(fd);
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_OK;
            }
        }
    } while (read_bytes > 0);
    /* Close file after sending complete */
    close(fd);
    ESP_LOGI(TAG, "File sending complete");
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t handle_options_request(httpd_req_t * req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    // Set CORS headers for OPTIONS request
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    // Send a blank response for OPTIONS request
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}

static esp_err_t PATCH_update_settings(httpd_req_t * req)
{

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    // Set CORS headers
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    int total_len = req->content_len;
    int cur_len = 0;
    char * buf = ((rest_server_context_t *) (req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_OK;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_OK;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON * root = cJSON_Parse(buf);
    cJSON * item;
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_OK;
    }

    if ((item = cJSON_GetObjectItem(root, "stratumURL")) != NULL) {
        nvs_config_set_string(NVS_CONFIG_STRATUM_URL, item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(root, "fallbackStratumURL")) != NULL) {
        nvs_config_set_string(NVS_CONFIG_FALLBACK_STRATUM_URL, item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(root, "stratumUser")) != NULL) {
        nvs_config_set_string(NVS_CONFIG_STRATUM_USER, item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(root, "stratumPassword")) != NULL) {
        nvs_config_set_string(NVS_CONFIG_STRATUM_PASS, item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(root, "fallbackStratumUser")) != NULL) {
        nvs_config_set_string(NVS_CONFIG_FALLBACK_STRATUM_USER, item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(root, "fallbackStratumPassword")) != NULL) {
        nvs_config_set_string(NVS_CONFIG_FALLBACK_STRATUM_PASS, item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(root, "stratumPort")) != NULL) {
        nvs_config_set_u16(NVS_CONFIG_STRATUM_PORT, item->valueint);
    }
    if ((item = cJSON_GetObjectItem(root, "fallbackStratumPort")) != NULL) {
        nvs_config_set_u16(NVS_CONFIG_FALLBACK_STRATUM_PORT, item->valueint);
    }
    if ((item = cJSON_GetObjectItem(root, "ssid")) != NULL) {
        nvs_config_set_string(NVS_CONFIG_WIFI_SSID, item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(root, "wifiPass")) != NULL) {
        nvs_config_set_string(NVS_CONFIG_WIFI_PASS, item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(root, "hostname")) != NULL) {
        nvs_config_set_string(NVS_CONFIG_HOSTNAME, item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(root, "coreVoltage")) != NULL && item->valueint > 0) {
        nvs_config_set_u16(NVS_CONFIG_ASIC_VOLTAGE, item->valueint);
    }
    if ((item = cJSON_GetObjectItem(root, "frequency")) != NULL && item->valueint > 0) {
        nvs_config_set_u16(NVS_CONFIG_ASIC_FREQ, item->valueint);
    }
    if ((item = cJSON_GetObjectItem(root, "flipscreen")) != NULL) {
        nvs_config_set_u16(NVS_CONFIG_FLIP_SCREEN, item->valueint);
    }
    if ((item = cJSON_GetObjectItem(root, "overheat_mode")) != NULL) {
        nvs_config_set_u16(NVS_CONFIG_OVERHEAT_MODE, 0);
    }
    if ((item = cJSON_GetObjectItem(root, "invertscreen")) != NULL) {
        nvs_config_set_u16(NVS_CONFIG_INVERT_SCREEN, item->valueint);
    }
    if ((item = cJSON_GetObjectItem(root, "invertfanpolarity")) != NULL) {
        nvs_config_set_u16(NVS_CONFIG_INVERT_FAN_POLARITY, item->valueint);
    }
    if ((item = cJSON_GetObjectItem(root, "autofanspeed")) != NULL) {
        nvs_config_set_u16(NVS_CONFIG_AUTO_FAN_SPEED, item->valueint);
    }
    if ((item = cJSON_GetObjectItem(root, "fanspeed")) != NULL) {
        nvs_config_set_u16(NVS_CONFIG_FAN_SPEED, item->valueint);
    }

    cJSON_Delete(root);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t POST_restart(httpd_req_t * req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    // Set CORS headers
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Restarting System because of API Request");

    // Send HTTP response before restarting
    const char* resp_str = "System will restart shortly.";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    // Delay to ensure the response is sent
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    // Restart the system
    esp_restart();

    // This return statement will never be reached, but it's good practice to include it
    return ESP_OK;
}

/* Simple handler for getting system handler */
static esp_err_t GET_system_info(httpd_req_t * req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    httpd_resp_set_type(req, "application/json");

    // Set CORS headers
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_OK;
    }


    char * ssid = nvs_config_get_string(NVS_CONFIG_WIFI_SSID, CONFIG_ESP_WIFI_SSID);
    char * hostname = nvs_config_get_string(NVS_CONFIG_HOSTNAME, CONFIG_LWIP_LOCAL_HOSTNAME);
    uint8_t mac[6];
    char formattedMac[18];
    char * stratumURL = nvs_config_get_string(NVS_CONFIG_STRATUM_URL, CONFIG_STRATUM_URL);
    char * fallbackStratumURL = nvs_config_get_string(NVS_CONFIG_FALLBACK_STRATUM_URL, CONFIG_FALLBACK_STRATUM_URL);
    char * stratumUser = nvs_config_get_string(NVS_CONFIG_STRATUM_USER, CONFIG_STRATUM_USER);
    char * fallbackStratumUser = nvs_config_get_string(NVS_CONFIG_FALLBACK_STRATUM_USER, CONFIG_FALLBACK_STRATUM_USER);
    char * board_version = nvs_config_get_string(NVS_CONFIG_BOARD_VERSION, "unknown");

    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(formattedMac, 18, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        cJSON * root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "power", GLOBAL_STATE->POWER_MANAGEMENT_MODULE.power);
    cJSON_AddNumberToObject(root, "voltage", GLOBAL_STATE->POWER_MANAGEMENT_MODULE.voltage);
    cJSON_AddNumberToObject(root, "current", GLOBAL_STATE->POWER_MANAGEMENT_MODULE.current);
    cJSON_AddNumberToObject(root, "temp", GLOBAL_STATE->POWER_MANAGEMENT_MODULE.chip_temp_avg);
    cJSON_AddNumberToObject(root, "vrTemp", GLOBAL_STATE->POWER_MANAGEMENT_MODULE.vr_temp);
    cJSON_AddNumberToObject(root, "hashRate", GLOBAL_STATE->SYSTEM_MODULE.current_hashrate);
    cJSON_AddStringToObject(root, "bestDiff", GLOBAL_STATE->SYSTEM_MODULE.best_diff_string);
    cJSON_AddStringToObject(root, "bestSessionDiff", GLOBAL_STATE->SYSTEM_MODULE.best_session_diff_string);
    cJSON_AddNumberToObject(root, "stratumDiff", GLOBAL_STATE->stratum_difficulty);

    cJSON_AddNumberToObject(root, "isUsingFallbackStratum", GLOBAL_STATE->SYSTEM_MODULE.is_using_fallback);

    cJSON_AddNumberToObject(root, "isPSRAMAvailable", GLOBAL_STATE->psram_is_available);

    cJSON_AddNumberToObject(root, "freeHeap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(root, "coreVoltage", nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE, CONFIG_ASIC_VOLTAGE));
    cJSON_AddNumberToObject(root, "coreVoltageActual", VCORE_get_voltage_mv(GLOBAL_STATE));
    cJSON_AddNumberToObject(root, "frequency", nvs_config_get_u16(NVS_CONFIG_ASIC_FREQ, CONFIG_ASIC_FREQUENCY));
    cJSON_AddStringToObject(root, "ssid", ssid);
    cJSON_AddStringToObject(root, "macAddr", formattedMac);
    cJSON_AddStringToObject(root, "hostname", hostname);
    cJSON_AddStringToObject(root, "wifiStatus", GLOBAL_STATE->SYSTEM_MODULE.wifi_status);
    cJSON_AddNumberToObject(root, "sharesAccepted", GLOBAL_STATE->SYSTEM_MODULE.shares_accepted);
    cJSON_AddNumberToObject(root, "sharesRejected", GLOBAL_STATE->SYSTEM_MODULE.shares_rejected);
    cJSON_AddNumberToObject(root, "uptimeSeconds", (esp_timer_get_time() - GLOBAL_STATE->SYSTEM_MODULE.start_time) / 1000000);
    cJSON_AddNumberToObject(root, "asicCount", GLOBAL_STATE->asic_count);
    uint16_t small_core_count = 0;
    switch (GLOBAL_STATE->asic_model){
        case ASIC_BM1397:
            small_core_count = BM1397_SMALL_CORE_COUNT;
            break;
        case ASIC_BM1366:
            small_core_count = BM1366_SMALL_CORE_COUNT;
            break;
        case ASIC_BM1368:
            small_core_count = BM1368_SMALL_CORE_COUNT;
            break;
        case ASIC_BM1370:
            small_core_count = BM1370_SMALL_CORE_COUNT;
            break;
        case ASIC_UNKNOWN:
        default:
            small_core_count = -1;
            break;
    }
    cJSON_AddNumberToObject(root, "smallCoreCount", small_core_count);
    cJSON_AddStringToObject(root, "ASICModel", GLOBAL_STATE->asic_model_str);
    cJSON_AddStringToObject(root, "stratumURL", stratumURL);
    cJSON_AddStringToObject(root, "fallbackStratumURL", fallbackStratumURL);
    cJSON_AddNumberToObject(root, "stratumPort", nvs_config_get_u16(NVS_CONFIG_STRATUM_PORT, CONFIG_STRATUM_PORT));
    cJSON_AddNumberToObject(root, "fallbackStratumPort", nvs_config_get_u16(NVS_CONFIG_FALLBACK_STRATUM_PORT, CONFIG_FALLBACK_STRATUM_PORT));
    cJSON_AddStringToObject(root, "stratumUser", stratumUser);
    cJSON_AddStringToObject(root, "fallbackStratumUser", fallbackStratumUser);

    cJSON_AddStringToObject(root, "version", esp_app_get_description()->version);
    cJSON_AddStringToObject(root, "idfVersion", esp_get_idf_version());
    cJSON_AddStringToObject(root, "boardVersion", board_version);
    cJSON_AddStringToObject(root, "runningPartition", esp_ota_get_running_partition()->label);

    cJSON_AddNumberToObject(root, "flipscreen", nvs_config_get_u16(NVS_CONFIG_FLIP_SCREEN, 1));
    cJSON_AddNumberToObject(root, "overheat_mode", nvs_config_get_u16(NVS_CONFIG_OVERHEAT_MODE, 0));
    cJSON_AddNumberToObject(root, "invertscreen", nvs_config_get_u16(NVS_CONFIG_INVERT_SCREEN, 0));

    cJSON_AddNumberToObject(root, "invertfanpolarity", nvs_config_get_u16(NVS_CONFIG_INVERT_FAN_POLARITY, 1));
    cJSON_AddNumberToObject(root, "autofanspeed", nvs_config_get_u16(NVS_CONFIG_AUTO_FAN_SPEED, 1));

    cJSON_AddNumberToObject(root, "fanspeed", GLOBAL_STATE->POWER_MANAGEMENT_MODULE.fan_perc);
    cJSON_AddNumberToObject(root, "fanrpm", GLOBAL_STATE->POWER_MANAGEMENT_MODULE.fan_rpm);

    free(ssid);
    free(hostname);
    free(stratumURL);
    free(fallbackStratumURL);
    free(stratumUser);
    free(fallbackStratumUser);
    free(board_version);

    const char * sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    free((char *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t POST_WWW_update(httpd_req_t * req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Not allowed in AP mode");
        return ESP_OK;
    }

    char buf[1000];
    int remaining = req->content_len;

    const esp_partition_t * www_partition =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "www");
    if (www_partition == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "WWW partition not found");
        return ESP_OK;
    }

    // Don't attempt to write more than what can be stored in the partition
    if (remaining > www_partition->size) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File provided is too large for device");
        return ESP_OK;
    }

    // Erase the entire www partition before writing
    ESP_ERROR_CHECK(esp_partition_erase_range(www_partition, 0, www_partition->size));

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));

        if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        } else if (recv_len <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
            return ESP_OK;
        }

        if (esp_partition_write(www_partition, www_partition->size - remaining, (const void *) buf, recv_len) != ESP_OK) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write Error");
            return ESP_OK;
        }

        remaining -= recv_len;
    }

    httpd_resp_sendstr(req, "WWW update complete\n");
    return ESP_OK;
}

/*
 * Handle OTA file upload
 */
esp_err_t POST_OTA_update(httpd_req_t * req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Not allowed in AP mode");
        return ESP_OK;
    }
    
    char buf[1000];
    esp_ota_handle_t ota_handle;
    int remaining = req->content_len;

    const esp_partition_t * ota_partition = esp_ota_get_next_update_partition(NULL);
    ESP_ERROR_CHECK(esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle));

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));

        // Timeout Error: Just retry
        if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;

            // Serious Error: Abort OTA
        } else if (recv_len <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
            return ESP_OK;
        }

        // Successful Upload: Flash firmware chunk
        if (esp_ota_write(ota_handle, (const void *) buf, recv_len) != ESP_OK) {
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash Error");
            return ESP_OK;
        }

        remaining -= recv_len;
    }

    // Validate and switch to new OTA image and reboot
    if (esp_ota_end(ota_handle) != ESP_OK || esp_ota_set_boot_partition(ota_partition) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Validation / Activation Error");
        return ESP_OK;
    }

    httpd_resp_sendstr(req, "Firmware update complete, rebooting now!\n");
    ESP_LOGI(TAG, "Restarting System because of Firmware update complete");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();

    return ESP_OK;
}

int log_to_queue(const char * format, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);

    // Calculate the required buffer size
    int needed_size = vsnprintf(NULL, 0, format, args_copy) + 1;
    va_end(args_copy);

    // Allocate the buffer dynamically
    char * log_buffer = (char *) calloc(needed_size + 2, sizeof(char));  // +2 for potential \n and \0
    if (log_buffer == NULL) {
        return 0;
    }

    // Format the string into the allocated buffer
    va_copy(args_copy, args);
    vsnprintf(log_buffer, needed_size, format, args_copy);
    va_end(args_copy);

    // Ensure the log message ends with a newline
    size_t len = strlen(log_buffer);
    if (len > 0 && log_buffer[len - 1] != '\n') {
        log_buffer[len] = '\n';
        log_buffer[len + 1] = '\0';
        len++;
    }

    // Print to standard output
    printf("%s", log_buffer);

    if (xQueueSendToBack(log_queue, (void*)&log_buffer, (TickType_t) 0) != pdPASS) {
        if (log_buffer != NULL) {
            free((void*)log_buffer);
        }
    }

    return 0;
}

void send_log_to_websocket(char *message)
{
    // Prepare the WebSocket frame
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)message;
    ws_pkt.len = strlen(message);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    // Ensure server and fd are valid
    if (server != NULL && fd >= 0) {
        // Send the WebSocket frame asynchronously
        if (httpd_ws_send_frame_async(server, fd, &ws_pkt) != ESP_OK) {
            esp_log_set_vprintf(vprintf);
        }
    }

    // Free the allocated buffer
    free((void*)message);
}

/*
 * This handler echos back the received ws data
 * and triggers an async send if certain message received
 */
esp_err_t echo_handler(httpd_req_t * req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        fd = httpd_req_to_sockfd(req);
        esp_log_set_vprintf(log_to_queue);
        return ESP_OK;
    }
    return ESP_OK;
}

// HTTP Error (404) Handler - Redirects all requests to the root page
esp_err_t http_404_error_handler(httpd_req_t * req, httpd_err_code_t err)
{
    // Set status
    httpd_resp_set_status(req, "302 Temporary Redirect");
    // Redirect to the "/" root directory
    httpd_resp_set_hdr(req, "Location", "/");
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Redirecting to root");
    return ESP_OK;
}

void websocket_log_handler()
{
    while (true)
    {
        char *message;
        if (xQueueReceive(log_queue, &message, (TickType_t) portMAX_DELAY) != pdPASS) {
            if (message != NULL) {
                free((void*)message);
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
            continue;
        }

        if (fd == -1) {
            free((void*)message);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }

        send_log_to_websocket(message);
    }
}

esp_err_t start_rest_server(void * pvParameters)
{
    GLOBAL_STATE = (GlobalState *) pvParameters;
    const char * base_path = "";

    bool enter_recovery = false;
    if (init_fs() != ESP_OK) {
        // Unable to initialize the web app filesystem.
        // Enter recovery mode
        enter_recovery = true;
    }

    REST_CHECK(base_path, "wrong base path", err);
    rest_server_context_t * rest_context = calloc(1, sizeof(rest_server_context_t));
    REST_CHECK(rest_context, "No memory for rest context", err);
    strlcpy(rest_context->base_path, base_path, sizeof(rest_context->base_path));

    log_queue = xQueueCreate(MESSAGE_QUEUE_SIZE, sizeof(char*));

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_open_sockets = 10;
    config.max_uri_handlers = 20;

    ESP_LOGI(TAG, "Starting HTTP Server");
    REST_CHECK(httpd_start(&server, &config) == ESP_OK, "Start server failed", err_start);

    httpd_uri_t recovery_explicit_get_uri = {
        .uri = "/recovery", 
        .method = HTTP_GET, 
        .handler = rest_recovery_handler, 
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &recovery_explicit_get_uri);
    
    // Register theme API endpoints
    ESP_ERROR_CHECK(register_theme_api_endpoints(server, rest_context));

    /* URI handler for fetching system info */
    httpd_uri_t system_info_get_uri = {
        .uri = "/api/system/info", 
        .method = HTTP_GET, 
        .handler = GET_system_info, 
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &system_info_get_uri);

    httpd_uri_t swarm_options_uri = {
        .uri = "/api/swarm",
        .method = HTTP_OPTIONS,
        .handler = handle_options_request,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &swarm_options_uri);

    httpd_uri_t system_restart_uri = {
        .uri = "/api/system/restart", .method = HTTP_POST, 
        .handler = POST_restart, 
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &system_restart_uri);

    httpd_uri_t system_restart_options_uri = {
        .uri = "/api/system/restart", 
        .method = HTTP_OPTIONS, 
        .handler = handle_options_request, 
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &system_restart_options_uri);

    httpd_uri_t update_system_settings_uri = {
        .uri = "/api/system", 
        .method = HTTP_PATCH, 
        .handler = PATCH_update_settings, 
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &update_system_settings_uri);

    httpd_uri_t system_options_uri = {
        .uri = "/api/system",
        .method = HTTP_OPTIONS,
        .handler = handle_options_request,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &system_options_uri);

    httpd_uri_t update_post_ota_firmware = {
        .uri = "/api/system/OTA", 
        .method = HTTP_POST, 
        .handler = POST_OTA_update, 
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &update_post_ota_firmware);

    httpd_uri_t update_post_ota_www = {
        .uri = "/api/system/OTAWWW", 
        .method = HTTP_POST, 
        .handler = POST_WWW_update, 
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &update_post_ota_www);

    httpd_uri_t ws = {
        .uri = "/api/ws", 
        .method = HTTP_GET, 
        .handler = echo_handler, 
        .user_ctx = NULL, 
        .is_websocket = true
    };
    httpd_register_uri_handler(server, &ws);

    if (enter_recovery) {
        /* Make default route serve Recovery */
        httpd_uri_t recovery_implicit_get_uri = {
            .uri = "/*", .method = HTTP_GET, 
            .handler = rest_recovery_handler, 
            .user_ctx = rest_context
        };
        httpd_register_uri_handler(server, &recovery_implicit_get_uri);

    } else {
        /* URI handler for getting web server files */
        httpd_uri_t common_get_uri = {
            .uri = "/*", 
            .method = HTTP_GET, 
            .handler = rest_common_get_handler, 
            .user_ctx = rest_context
        };
        httpd_register_uri_handler(server, &common_get_uri);
    }

    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);

    // Start websocket log handler thread
    xTaskCreate(&websocket_log_handler, "websocket_log_handler", 4096, NULL, 2, NULL);

    // Start the DNS server that will redirect all queries to the softAP IP
    dns_server_config_t dns_config = DNS_SERVER_CONFIG_SINGLE("*" /* all A queries */, "WIFI_AP_DEF" /* softAP netif ID */);
    start_dns_server(&dns_config);

    return ESP_OK;
err_start:
    free(rest_context);
err:
    return ESP_FAIL;
}
