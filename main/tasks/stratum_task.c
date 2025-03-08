#include "esp_log.h"
// #include "addr_from_stdin.h"
#include "bm1397.h"
#include "connect.h"
#include "system.h"
#include "global_state.h"
#include "lwip/dns.h"
#include <lwip/tcpip.h>
#include "nvs_config.h"
#include "stratum_task.h"
#include "work_queue.h"
#include "esp_wifi.h"
#include <esp_sntp.h>
#include <time.h>

#define PORT CONFIG_STRATUM_PORT
#define STRATUM_URL CONFIG_STRATUM_URL

#define FALLBACK_PORT CONFIG_FALLBACK_STRATUM_PORT
#define FALLBACK_STRATUM_URL CONFIG_FALLBACK_STRATUM_URL

#define STRATUM_PW CONFIG_STRATUM_PW
#define FALLBACK_STRATUM_PW CONFIG_FALLBACK_STRATUM_PW
#define STRATUM_DIFFICULTY CONFIG_STRATUM_DIFFICULTY

static const char * TAG = "stratum_task";

static int addr_family = AF_INET;
static int ip_protocol = IPPROTO_IP;

void stratum_process(const char * POOL_TAG, GlobalState * GLOBAL_STATE, StratumConnection * connection);
void stratum_task(const char * POOL_TAG, GlobalState * GLOBAL_STATE, StratumConnection * connection);
void stratum_task_init_connection(StratumConnection * connection);

struct timeval tcp_snd_timeout = {
    .tv_sec = 5,
    .tv_usec = 0
};

struct timeval tcp_rcv_timeout = {
    .tv_sec = 60 * 10,
    .tv_usec = 0
};

bool is_wifi_connected() {
    wifi_ap_record_t ap_info;
    return esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK;
}

void stratum_task_primary(void * pvParameters)
{
    GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;

    StratumConnection * connection = &GLOBAL_STATE->connections[0];

    stratum_task_init_connection(connection);
    connection->id = 0;
    connection->host = GLOBAL_STATE->SYSTEM_MODULE.pool_url;
    connection->port = GLOBAL_STATE->SYSTEM_MODULE.pool_port;
    connection->username = GLOBAL_STATE->SYSTEM_MODULE.pool_user;
    connection->password = GLOBAL_STATE->SYSTEM_MODULE.pool_pass;

    ESP_LOGI(TAG, "Opening connection to primary pool: %s:%d", connection->host, connection->port);

    stratum_task("primary_pool", GLOBAL_STATE, connection);
}

void stratum_task_secondary(void * pvParameters)
{
    GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;

    StratumConnection * connection = &GLOBAL_STATE->connections[1];

    stratum_task_init_connection(connection);
    connection->id = 1;
    connection->host = GLOBAL_STATE->SYSTEM_MODULE.fallback_pool_url;
    connection->port = GLOBAL_STATE->SYSTEM_MODULE.fallback_pool_port;
    connection->username = GLOBAL_STATE->SYSTEM_MODULE.fallback_pool_user;
    connection->password = GLOBAL_STATE->SYSTEM_MODULE.fallback_pool_pass;

    ESP_LOGI(TAG, "Opening connection to secondary pool: %s:%d", connection->host, connection->port);

    stratum_task("secondary_pool", GLOBAL_STATE, connection);
}

void stratum_task_watchdog(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting Stratum Watchdog.");

    GlobalState *GLOBAL_STATE = (GlobalState *)pvParameters;

    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        // Always fail back over to the Primary when it is back up.
        if (GLOBAL_STATE->current_connection_id != 0 &&
            GLOBAL_STATE->connections[0].state == STRATUM_CONNECTED)
        {
            GLOBAL_STATE->current_connection_id = 0;
            GLOBAL_STATE->connections[0].abandon_work = 0;
            GLOBAL_STATE->connections[0].new_stratum_version_rolling_msg = true;
            GLOBAL_STATE->SYSTEM_MODULE.is_using_fallback = false;
            ESP_LOGI(TAG, "Switching back to primary pool.");
            continue;
        }

        // No need to do anything if the current connection is stable.
        if (GLOBAL_STATE->connections[GLOBAL_STATE->current_connection_id].state == STRATUM_CONNECTED)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        // Loop through all connections to try to find a healthy one.
        for (int i = 1; i < MAX_STRATUM_POOLS; i++)
        {
            if (GLOBAL_STATE->current_connection_id == i)
                continue;

            // Lets wait until this pool is done retrying before moving forward.
            if (GLOBAL_STATE->connections[i].state == STRATUM_CONNECTING)
            {
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                break;
            }

            if (GLOBAL_STATE->connections[i].state == STRATUM_CONNECTED)
            {
                GLOBAL_STATE->current_connection_id = i;
                GLOBAL_STATE->connections[i].abandon_work = 0;
                GLOBAL_STATE->connections[i].new_stratum_version_rolling_msg = true;
                ESP_LOGI(TAG, "Switching to pool: %s", GLOBAL_STATE->connections[i].host);
                GLOBAL_STATE->SYSTEM_MODULE.is_using_fallback = true;
                break;
            }
        }
    }
}

void stratum_reset_uid(StratumConnection * connection)
{
    connection->send_uid = 1;
}

void stratum_close_connection(StratumConnection * connection)
{
    if (connection->sock < 0)
    {
        ESP_LOGE(TAG, "Socket already shutdown, not shutting down again..");
        return;
    }

    close(connection->sock);
}

void stratum_clear_queue(const char * POOL_TAG, StratumConnection * connection)
{
    ESP_LOGD(POOL_TAG, "Clearing queues for connection.");

    connection->abandon_work = 1;
    queue_clear(&connection->stratum_queue);

    pthread_mutex_lock(&connection->jobs_lock);
    for (int i = 0; i < 128; i = i + 4) {
        connection->jobs[i] = 0;
    }
    pthread_mutex_unlock(&connection->jobs_lock);
    taskYIELD();
}

void stratum_task_init_connection(StratumConnection * connection)
{
    queue_init(&connection->stratum_queue);
    connection->extranonce_str = NULL,
    connection->extranonce_2_len = 0,
    connection->version_mask = 0,
    connection->abandon_work = 0;
    connection->buf = STRATUM_V1_buffer_create();
    connection->stratum_difficulty = 1024;
    connection->send_uid = 1;
    connection->sock = -1;
    connection->retry_attempts = 0;
    connection->state = STRATUM_CONNECTING;
    connection->message = malloc(sizeof(StratumApiV1Message));
    connection->jobs = malloc(sizeof(uint8_t) * 128);
    for (int i = 0; i < 128; i++)
    {
        connection->jobs[i] = 0;
    }
}

void stratum_handle_disconnect(const char * POOL_TAG, StratumConnection * connection)
{
    stratum_close_connection(connection);

    if (connection->state == STRATUM_CONNECTED)
    {
        ESP_LOGE(POOL_TAG, "Pool disconnected.");
    }

    stratum_clear_queue(POOL_TAG, connection);

    connection->retry_attempts++;

    if (connection->retry_attempts <= 3)
    {
        connection->state = STRATUM_CONNECTING;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    else
    {
        int delay_ms = 1000 + (connection->retry_attempts * 1000);
        if (delay_ms > 30 * 1000) delay_ms = 30000;

        connection->state = STRATUM_DISCONNECTED;
        vTaskDelay(delay_ms / portTICK_PERIOD_MS);
    }
}

void stratum_task(const char * POOL_TAG, GlobalState * GLOBAL_STATE, StratumConnection * connection)
{
    while (1) {
        connection->state = STRATUM_CONNECTING;
        connection->send_uid = 1;

        if (!is_wifi_connected()) {
            ESP_LOGI(POOL_TAG, "WiFi disconnected, attempting to reconnect...");
            connection->state = STRATUM_CONNECTING;
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            continue;
        }

        struct hostent *dns_addr = gethostbyname(connection->host);
        if (dns_addr == NULL) {
            ESP_LOGE(POOL_TAG, "Unable to resolve DNS.");
            stratum_handle_disconnect(POOL_TAG, connection);
            continue;
        }

        inet_ntop(AF_INET, (void *)dns_addr->h_addr_list[0], connection->ip_address, sizeof(connection->ip_address));

        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(connection->ip_address);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(connection->port);

        connection->sock = socket(addr_family, SOCK_STREAM, ip_protocol);
        if (connection->sock < 0) {
            ESP_LOGE(POOL_TAG, "Unable to create the socket.");
            close(connection->sock);
            stratum_handle_disconnect(POOL_TAG, connection);
            continue;
        }

        int err = connect(connection->sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in6));
        if (err != 0)
        {
            ESP_LOGE(POOL_TAG, "Unable to connect to socket.");
            close(connection->sock);
            stratum_handle_disconnect(POOL_TAG, connection);
            continue;
        }

        if (setsockopt(connection->sock, SOL_SOCKET, SO_SNDTIMEO, &tcp_snd_timeout, sizeof(tcp_snd_timeout)) != 0) {
            ESP_LOGE(POOL_TAG, "Fail to setsockopt SO_SNDTIMEO");
        }

        if (setsockopt(connection->sock, SOL_SOCKET, SO_RCVTIMEO , &tcp_rcv_timeout, sizeof(tcp_rcv_timeout)) != 0) {
            ESP_LOGE(POOL_TAG, "Fail to setsockopt SO_RCVTIMEO ");
        }

        stratum_reset_uid(connection);

        STRATUM_V1_configure_version_rolling(POOL_TAG, connection->sock, connection->send_uid++);
        STRATUM_V1_subscribe(POOL_TAG, connection->sock, connection->send_uid++, GLOBAL_STATE->asic_model_str);
        STRATUM_V1_authenticate(POOL_TAG, connection->sock, connection->send_uid++, connection->username, connection->password);
        STRATUM_V1_suggest_difficulty(POOL_TAG, connection->sock, connection->send_uid++, STRATUM_DIFFICULTY);

        connection->abandon_work = 0;

        stratum_process(POOL_TAG, GLOBAL_STATE, connection);

        if (GLOBAL_STATE->current_connection_id == connection->id)
            ASIC_jobs_queue_clear(&GLOBAL_STATE->ASIC_jobs_queue);
    }
}

void stratum_process(const char * POOL_TAG, GlobalState * GLOBAL_STATE, StratumConnection * connection)
{
    while (1) {
        char * line = STRATUM_V1_receive_jsonrpc_line(POOL_TAG, connection->sock, connection->buf);
        if (!line) {
            ESP_LOGE(POOL_TAG, "Failed to receive JSON-RPC line, reconnecting...");
            stratum_handle_disconnect(POOL_TAG, connection);
            break;
        }

        ESP_LOGI(POOL_TAG, "rx: %s", line);
        STRATUM_V1_parse(POOL_TAG, connection->message, line);
        free(line);

        if (connection->message->method == MINING_NOTIFY)
        {
            if (GLOBAL_STATE->current_connection_id == connection->id)
                SYSTEM_notify_new_ntime(GLOBAL_STATE, connection->message->mining_notification->ntime);

            if (connection->message->should_abandon_work &&
                (connection->stratum_queue.count > 0 || GLOBAL_STATE->ASIC_jobs_queue.count > 0))
            {
                stratum_clear_queue(POOL_TAG, connection);
                ESP_LOGI(POOL_TAG, "Abandoning the Stratum queues.");
            }
            if (connection->stratum_queue.count >= QUEUE_SIZE)
            {
                mining_notify * next_notify_json_str = (mining_notify *) queue_dequeue(&connection->stratum_queue);
                STRATUM_V1_free_mining_notify(next_notify_json_str);
            }

            connection->message->mining_notification->difficulty = connection->stratum_difficulty;
            connection->message->mining_notification->connection_id = connection->id;
            queue_enqueue(&connection->stratum_queue, connection->message->mining_notification);
        }
        else if (connection->message->method == MINING_SET_DIFFICULTY)
        {
            if (connection->stratum_difficulty != connection->message->new_difficulty)
            {
                connection->stratum_difficulty = connection->message->new_difficulty;
                ESP_LOGI(POOL_TAG, "Set stratum difficulty: %ld", connection->stratum_difficulty);
            }
        }
        else if (connection->message->method == MINING_SET_VERSION_MASK ||
                connection->message->method == STRATUM_RESULT_VERSION_MASK)
        {
            ESP_LOGI(POOL_TAG, "Set version mask: %08lx", connection->message->version_mask);
            connection->version_mask = connection->message->version_mask;
            connection->new_stratum_version_rolling_msg = true;
        }
        else if (connection->message->method == STRATUM_RESULT_SUBSCRIBE)
        {
            connection->extranonce_str = connection->message->extranonce_str;
            connection->extranonce_2_len = connection->message->extranonce_2_len;
        }
        else if (connection->message->method == CLIENT_RECONNECT)
        {
            ESP_LOGE(POOL_TAG, "Pool requested client reconnect...");
            stratum_clear_queue(POOL_TAG, connection);
            stratum_close_connection(connection);
            break;
        }
        else if (connection->message->method == STRATUM_RESULT)
        {
            if (connection->message->response_success)
            {
                ESP_LOGI(POOL_TAG, "message result accepted");
                SYSTEM_notify_accepted_share(GLOBAL_STATE);
            }
            else
            {
                ESP_LOGW(POOL_TAG, "message result rejected: %s", connection->message->error_str);
                SYSTEM_notify_rejected_share(GLOBAL_STATE, connection->message->error_str);
            }
        }
        else if (connection->message->method == STRATUM_RESULT_SETUP)
        {
            if (connection->message->response_success)
            {
                connection->retry_attempts = 0;
                connection->state = STRATUM_CONNECTED;

                ESP_LOGI(POOL_TAG, "setup message accepted");
            }
            else
            {
                ESP_LOGE(POOL_TAG, "setup message rejected: %s", connection->message->error_str);
            }
        }
    }
}
