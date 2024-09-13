#include <esp_sntp.h>
#include <time.h>
#include "esp_log.h"
// #include "addr_from_stdin.h"

#include "lwip/dns.h"
#include "esp_wifi.h"

#include "bm1397.h"
#include "connect.h"
#include "system.h"
#include "global_state.h"
#include "nvs_config.h"
#include "work_queue.h"
#include "stratum_task.h"

#define PORT CONFIG_STRATUM_PORT
#define STRATUM_URL CONFIG_STRATUM_URL

#define STRATUM_PW CONFIG_STRATUM_PW
#define STRATUM_DIFFICULTY CONFIG_STRATUM_DIFFICULTY

#define MAX_RETRY_ATTEMPTS 5
static const char * TAG = "stratum_task";

static StratumApiV1Message stratum_api_v1_message = {};

static SystemTaskModule SYSTEM_TASK_MODULE = {.stratum_difficulty = 8192};


bool is_wifi_connected() {
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        return true;
    } else {
        return false;
    }
}

void cleanQueue(GlobalState * GLOBAL_STATE) {
    ESP_LOGI(TAG, "Clean Jobs: clearing queue");
    GLOBAL_STATE->abandon_work = 1;
    queue_clear(&GLOBAL_STATE->stratum_queue);

    pthread_mutex_lock(&GLOBAL_STATE->valid_jobs_lock);
    ASIC_jobs_queue_clear(&GLOBAL_STATE->ASIC_jobs_queue);
    for (int i = 0; i < 128; i = i + 4) {
        GLOBAL_STATE->valid_jobs[i] = 0;
    }
    pthread_mutex_unlock(&GLOBAL_STATE->valid_jobs_lock);
}

//function stratum_connect to open the socket
esp_err_t Stratum_socket_connect(GlobalState * GLOBAL_STATE) {
    char *stratum_url = GLOBAL_STATE->SYSTEM_MODULE.pool_url;
    uint16_t port = GLOBAL_STATE->SYSTEM_MODULE.pool_port;

    if (!is_wifi_connected()) {
        ESP_LOGI(TAG, "WiFi is not connected");
        return ESP_ERR_WIFI_BASE;
    }

    char host_ip[INET_ADDRSTRLEN];
    struct hostent *dns_addr = gethostbyname(stratum_url);
    if (dns_addr == NULL) {
        ESP_LOGE(TAG, "DNS lookup failed for URL: %s", stratum_url);
        return ESP_FAIL;
    }
    inet_ntop(AF_INET, (void *)dns_addr->h_addr_list[0], host_ip, sizeof(host_ip));

    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(host_ip);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);

    GLOBAL_STATE->sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (GLOBAL_STATE->sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d: %s", errno, strerror(errno));
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Connecting to: stratum+tcp://%s:%d (%s)", stratum_url, port, host_ip);
    int err = connect(GLOBAL_STATE->sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in6));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to connect to %s:%d (errno %d: %s)", stratum_url, port, errno, strerror(errno));
        shutdown(GLOBAL_STATE->sock, SHUT_RDWR);
        close(GLOBAL_STATE->sock);
        return ESP_FAIL;
    }

    struct timeval timeout = {};
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    if (setsockopt(GLOBAL_STATE->sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0) {
        ESP_LOGE(TAG, "Fail to setsockopt SO_SNDTIMEO");
    }

    ESP_LOGI(TAG, "Successfully connected to %s:%d", stratum_url, port);
    return ESP_OK;
}

void stratum_task(void * pvParameters)
{
    GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;

    STRATUM_V1_initialize_buffer(); //we should cleanup after this with cleanup_stratum_buffer()
    STRATUM_V1_reset_uid();
    //cleanQueue(GLOBAL_STATE);

    ///// Start Stratum Action
    // mining.subscribe - ID: 1
    STRATUM_V1_subscribe(GLOBAL_STATE->sock, GLOBAL_STATE->asic_model_str);

    // mining.configure - ID: 2
    STRATUM_V1_configure_version_rolling(GLOBAL_STATE->sock, &GLOBAL_STATE->version_mask);

    //mining.suggest_difficulty - ID: 3
    STRATUM_V1_suggest_difficulty(GLOBAL_STATE->sock, STRATUM_DIFFICULTY);

    char * username = nvs_config_get_string(NVS_CONFIG_STRATUM_USER, STRATUM_USER);
    char * password = nvs_config_get_string(NVS_CONFIG_STRATUM_PASS, STRATUM_PW);

    //mining.authorize - ID: 4
    STRATUM_V1_authenticate(GLOBAL_STATE->sock, username, password);
    free(password);
    free(username);

    while (1) {
        char * line = STRATUM_V1_receive_jsonrpc_line(GLOBAL_STATE->sock);
        if (!line) {
            ESP_LOGE(TAG, "Failed to receive JSON-RPC line, reconnecting...");
            shutdown(GLOBAL_STATE->sock, SHUT_RDWR);
            close(GLOBAL_STATE->sock);
            vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay before attempting to reconnect
            break;
        }
        ESP_LOGI(TAG, "rx: %s", line); // debug incoming stratum messages
        STRATUM_V1_parse(&stratum_api_v1_message, line);
        free(line);

        if (stratum_api_v1_message.method == MINING_NOTIFY) {
            SYSTEM_notify_new_ntime(GLOBAL_STATE, stratum_api_v1_message.mining_notification->ntime);
            if (stratum_api_v1_message.should_abandon_work &&
                (GLOBAL_STATE->stratum_queue.count > 0 || GLOBAL_STATE->ASIC_jobs_queue.count > 0)) {
                cleanQueue(GLOBAL_STATE);
            }
            if (GLOBAL_STATE->stratum_queue.count == QUEUE_SIZE) {
                mining_notify * next_notify_json_str = (mining_notify *) queue_dequeue(&GLOBAL_STATE->stratum_queue);
                STRATUM_V1_free_mining_notify(next_notify_json_str);
            }

            stratum_api_v1_message.mining_notification->difficulty = SYSTEM_TASK_MODULE.stratum_difficulty;
            queue_enqueue(&GLOBAL_STATE->stratum_queue, stratum_api_v1_message.mining_notification);
        } else if (stratum_api_v1_message.method == MINING_SET_DIFFICULTY) {
            if (stratum_api_v1_message.new_difficulty != SYSTEM_TASK_MODULE.stratum_difficulty) {
                SYSTEM_TASK_MODULE.stratum_difficulty = stratum_api_v1_message.new_difficulty;
                ESP_LOGI(TAG, "Set stratum difficulty: %ld", SYSTEM_TASK_MODULE.stratum_difficulty);
            }
        } else if (stratum_api_v1_message.method == MINING_SET_VERSION_MASK ||
                stratum_api_v1_message.method == STRATUM_RESULT_VERSION_MASK) {
            // 1fffe000
            ESP_LOGI(TAG, "Set version mask: %08lx", stratum_api_v1_message.version_mask);
            GLOBAL_STATE->version_mask = stratum_api_v1_message.version_mask;
        } else if (stratum_api_v1_message.method == STRATUM_RESULT_SUBSCRIBE) {
            GLOBAL_STATE->extranonce_str = stratum_api_v1_message.extranonce_str;
            GLOBAL_STATE->extranonce_2_len = stratum_api_v1_message.extranonce_2_len;
        } else if (stratum_api_v1_message.method == CLIENT_RECONNECT) {
            ESP_LOGE(TAG, "Pool requested client reconnect...");
            shutdown(GLOBAL_STATE->sock, SHUT_RDWR);
            close(GLOBAL_STATE->sock);
            vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay before attempting to reconnect
            break;
        } else if (stratum_api_v1_message.method == STRATUM_RESULT) {
            if (stratum_api_v1_message.response_success) {
                ESP_LOGI(TAG, "message result accepted");
                SYSTEM_notify_accepted_share(GLOBAL_STATE);
            } else {
                ESP_LOGW(TAG, "message result rejected");
                SYSTEM_notify_rejected_share(GLOBAL_STATE);
            }
        } else if (stratum_api_v1_message.method == STRATUM_RESULT_SETUP) {
            if (stratum_api_v1_message.response_success) {
                ESP_LOGI(TAG, "setup message accepted");
            } else {
                ESP_LOGE(TAG, "setup message rejected");
            }
        }
    }

    if (GLOBAL_STATE->sock != -1) {
        ESP_LOGE(TAG, "Shutting down socket and restarting...");
        shutdown(GLOBAL_STATE->sock, 0);
        close(GLOBAL_STATE->sock);
    }
    vTaskDelete(NULL);
}