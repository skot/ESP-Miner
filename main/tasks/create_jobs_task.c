#include <sys/time.h>
#include <limits.h>

#include "work_queue.h"
#include "global_state.h"
#include "esp_log.h"
#include "esp_system.h"
#include "mining.h"
#include "string.h"

#include "asic.h"

static const char *TAG = "create_jobs_task";

#define QUEUE_LOW_WATER_MARK 10 // Adjust based on your requirements

static bool should_generate_more_work(GlobalState *GLOBAL_STATE);
static void generate_work(GlobalState *GLOBAL_STATE, StratumConnection *active_connection, mining_notify *notification, uint32_t extranonce_2);

void create_jobs_task(void *pvParameters)
{
    GlobalState *GLOBAL_STATE = (GlobalState *)pvParameters;

    while (1)
    {
        uint16_t current_connection_id = GLOBAL_STATE->current_connection_id;
        StratumConnection *active_connection = &GLOBAL_STATE->connections[current_connection_id];

        if (active_connection->state != STRATUM_CONNECTED)
        {
            ESP_LOGD(TAG, "Connection ID %d not ready.", current_connection_id);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        mining_notify *mining_notification = (mining_notify *)queue_dequeue(&active_connection->stratum_queue);
        if (mining_notification == NULL)
        {
            ESP_LOGE(TAG, "Failed to dequeue mining notification");
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(TAG, "New Work Dequeued %s", mining_notification->job_id);

        if (active_connection->new_stratum_version_rolling_msg) {
            ESP_LOGI(TAG, "Set chip version rolls %i", (int)(active_connection->version_mask >> 13));
            ASIC_set_version_mask(GLOBAL_STATE, active_connection->version_mask);
            active_connection->new_stratum_version_rolling_msg = false;
        }

        uint32_t extranonce_2 = 0;
        while (active_connection->stratum_queue.count == 0 && active_connection->abandon_work == 0)
        {
            if (active_connection->state != STRATUM_CONNECTED)
                break;
            else if (current_connection_id != GLOBAL_STATE->current_connection_id)
                break;

            if (should_generate_more_work(GLOBAL_STATE))
            {
                generate_work(GLOBAL_STATE, active_connection, mining_notification, extranonce_2);

                // Increase extranonce_2 for the next job.
                extranonce_2++;
            }
            else
            {
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
        }

        if (active_connection->abandon_work == 1)
        {
            active_connection->abandon_work = 0;
            ASIC_jobs_queue_clear(&GLOBAL_STATE->ASIC_jobs_queue);
            xSemaphoreGive(GLOBAL_STATE->ASIC_TASK_MODULE.semaphore);
        }

        STRATUM_V1_free_mining_notify(mining_notification);
    }
}

static bool should_generate_more_work(GlobalState *GLOBAL_STATE)
{
    return GLOBAL_STATE->ASIC_jobs_queue.count < QUEUE_LOW_WATER_MARK;
}

static void generate_work(GlobalState *GLOBAL_STATE, StratumConnection *active_connection, mining_notify *notification, uint32_t extranonce_2)
{
    char *extranonce_2_str = extranonce_2_generate(extranonce_2, active_connection->extranonce_2_len);
    if (extranonce_2_str == NULL) {
        ESP_LOGE(TAG, "Failed to generate extranonce_2");
        return;
    }

    if (active_connection->extranonce_str == NULL)
    {
        ESP_LOGW(TAG, "active_connection->extranonce_str == NULL");
        return;
    }

    char *coinbase_tx = construct_coinbase_tx(notification->coinbase_1, notification->coinbase_2, active_connection->extranonce_str, extranonce_2_str);
    if (coinbase_tx == NULL) {
        ESP_LOGE(TAG, "Failed to construct coinbase_tx");
        free(extranonce_2_str);
        return;
    }

    char *merkle_root = calculate_merkle_root_hash(coinbase_tx, (uint8_t(*)[32])notification->merkle_branches, notification->n_merkle_branches);
    if (merkle_root == NULL) {
        ESP_LOGE(TAG, "Failed to calculate merkle_root");
        free(extranonce_2_str);
        free(coinbase_tx);
        return;
    }

    bm_job next_job = construct_bm_job(notification, merkle_root, active_connection->version_mask);

    bm_job *queued_next_job = malloc(sizeof(bm_job));
    if (queued_next_job == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for queued_next_job");
        free(extranonce_2_str);
        free(coinbase_tx);
        free(merkle_root);
        return;
    }

    memcpy(queued_next_job, &next_job, sizeof(bm_job));
    queued_next_job->extranonce2 = extranonce_2_str; // Transfer ownership
    queued_next_job->jobid = strdup(notification->job_id);
    queued_next_job->version_mask = active_connection->version_mask;

    queue_enqueue(&GLOBAL_STATE->ASIC_jobs_queue, queued_next_job);

    free(coinbase_tx);
    free(merkle_root);
}