#include "esp_log.h"
// #include "addr_from_stdin.h"
#include "bm1397.h"
#include "connect.h"
#include "global_state.h"
#include "lwip/dns.h"
#include "nvs_config.h"
#include "stratum_task.h"
#include "work_queue.h"
#include <esp_sntp.h>
#include <time.h>

#define PORT CONFIG_STRATUM_PORT
#define STRATUM_URL CONFIG_STRATUM_URL

#define STRATUM_PW CONFIG_STRATUM_PW
#define STRATUM_DIFFICULTY CONFIG_STRATUM_DIFFICULTY

static const char * TAG = "stratum_task";
static ip_addr_t ip_Addr;
static bool bDNSFound = false;
static bool bDNSInvalid = false;

static StratumApiV1Message stratum_api_v1_message = {};

static SystemTaskModule SYSTEM_TASK_MODULE = {.stratum_difficulty = 8192};

void stratum_task(void * pvParameters)
{
    GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;

    STRATUM_V1_initialize_buffer();
    char host_ip[20];
    int addr_family = 0;
    int ip_protocol = 0;

    char * stratum_url = GLOBAL_STATE->SYSTEM_MODULE.pool_url;
    uint16_t port = GLOBAL_STATE->SYSTEM_MODULE.pool_port;
    uint8_t is_tls = GLOBAL_STATE->SYSTEM_MODULE.is_tls;
    char * stratum_cert = GLOBAL_STATE->SYSTEM_MODULE.pool_cert;

    GLOBAL_STATE->transport = STRATUM_V1_transport_init(is_tls, stratum_cert);
    if(GLOBAL_STATE->transport == NULL) {
        ESP_LOGE(TAG, "Transport initialization failed.");
        vTaskDelete(NULL);
    }
    if(is_tls)
        ESP_LOGI(TAG, "Connecting to: stratum+tls://%s:%d\n", stratum_url, port);
    else
        ESP_LOGI(TAG, "Connecting to: stratum+tcp://%s:%d\n", stratum_url, port);


    while (1) {
        esp_err_t ret = STRATUM_V1_transport_connect(stratum_url, port, GLOBAL_STATE->transport);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Socket unable to connect to %s:%d (errno %d)", stratum_url, port, ret);
            // close the socket
            STRATUM_V1_transport_close(GLOBAL_STATE->transport);
            // instead of restarting, retry this every 5 seconds
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }

        STRATUM_V1_configure_version_rolling(GLOBAL_STATE->transport, &GLOBAL_STATE->version_mask);

        STRATUM_V1_subscribe(GLOBAL_STATE->transport, &GLOBAL_STATE->extranonce_str, &GLOBAL_STATE->extranonce_2_len,
                             GLOBAL_STATE->asic_model);

        // This should come before the final step of authenticate so the first job is sent with the proper difficulty set
        STRATUM_V1_suggest_difficulty(GLOBAL_STATE->transport, STRATUM_DIFFICULTY);

        char * username = nvs_config_get_string(NVS_CONFIG_STRATUM_USER, STRATUM_USER);
        STRATUM_V1_authenticate(GLOBAL_STATE->transport, username);
        free(username);

        ESP_LOGI(TAG, "Extranonce: %s", GLOBAL_STATE->extranonce_str);
        ESP_LOGI(TAG, "Extranonce 2 length: %d", GLOBAL_STATE->extranonce_2_len);

        while (1) {
            char * line = STRATUM_V1_receive_jsonrpc_line(GLOBAL_STATE->transport);
            ESP_LOGI(TAG, "rx: %s", line); // debug incoming stratum messages
            STRATUM_V1_parse(&stratum_api_v1_message, line);
            free(line);

            if (stratum_api_v1_message.method == MINING_NOTIFY) {
                SYSTEM_notify_new_ntime(&GLOBAL_STATE->SYSTEM_MODULE, stratum_api_v1_message.mining_notification->ntime);
                if (stratum_api_v1_message.should_abandon_work &&
                    (GLOBAL_STATE->stratum_queue.count > 0 || GLOBAL_STATE->ASIC_jobs_queue.count > 0)) {
                    ESP_LOGI(TAG, "abandoning work");

                    GLOBAL_STATE->abandon_work = 1;
                    queue_clear(&GLOBAL_STATE->stratum_queue);

                    pthread_mutex_lock(&GLOBAL_STATE->valid_jobs_lock);
                    ASIC_jobs_queue_clear(&GLOBAL_STATE->ASIC_jobs_queue);
                    for (int i = 0; i < 128; i = i + 4) {
                        GLOBAL_STATE->valid_jobs[i] = 0;
                    }
                    pthread_mutex_unlock(&GLOBAL_STATE->valid_jobs_lock);
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
            } else if (stratum_api_v1_message.method == STRATUM_RESULT) {
                if (stratum_api_v1_message.response_success) {
                    ESP_LOGI(TAG, "message result accepted");
                    SYSTEM_notify_accepted_share(&GLOBAL_STATE->SYSTEM_MODULE);
                } else {
                    ESP_LOGE(TAG, "message result rejected");
                    SYSTEM_notify_rejected_share(&GLOBAL_STATE->SYSTEM_MODULE);
                }
            }
        }

        if (GLOBAL_STATE->transport != NULL) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            STRATUM_V1_transport_close(GLOBAL_STATE->transport);
        }
    }
    vTaskDelete(NULL);
}
