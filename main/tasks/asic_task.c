#include "system.h"
#include "work_queue.h"
#include "serial.h"
#include "bm1397.h"
#include <string.h>
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "asic.h"

static const char *TAG = "ASIC_task";

// static bm_job ** active_jobs; is required to keep track of the active jobs since the

void ASIC_task(void *pvParameters)
{
    GlobalState *GLOBAL_STATE = (GlobalState *)pvParameters;

    //initialize the semaphore
    GLOBAL_STATE->ASIC_TASK_MODULE.semaphore = xSemaphoreCreateBinary();

    GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs = malloc(sizeof(bm_job *) * 128);
    // GLOBAL_STATE->stratum_context.jobs = malloc(sizeof(uint8_t) * 128);
    for (int i = 0; i < 128; i++)
    {
        GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[i] = NULL;
        // GLOBAL_STATE->stratum_context.jobs[i] = 0;
    }

    ESP_LOGI(TAG, "ASIC Job Interval: %.2f ms", GLOBAL_STATE->asic_job_frequency_ms);
    SYSTEM_notify_mining_started(GLOBAL_STATE);
    ESP_LOGI(TAG, "ASIC Ready!");

    while (1)
    {
        bm_job *next_bm_job = (bm_job *)queue_dequeue(&GLOBAL_STATE->ASIC_jobs_queue);
        StratumConnection *current_connection = &GLOBAL_STATE->connections[next_bm_job->connection_id];

        if (next_bm_job->connection_id != GLOBAL_STATE->current_connection_id)
            continue;

        if (current_connection->state != STRATUM_CONNECTED)
        {
            ESP_LOGW(TAG, "Connection for job no longer open. Skipping.");
            continue;
        }

        if (current_connection->stratum_difficulty != next_bm_job->pool_diff)
        {
            ESP_LOGI(TAG, "New pool difficulty %lu", next_bm_job->pool_diff);
            current_connection->stratum_difficulty = next_bm_job->pool_diff;
        }

        pthread_mutex_lock(&current_connection->jobs_lock);
        int job_id = ASIC_send_work(GLOBAL_STATE, next_bm_job);
        current_connection->jobs[job_id] = 1;
        pthread_mutex_unlock(&current_connection->jobs_lock);

        // Time to execute the above code is ~0.3ms
        // Delay for ASIC(s) to finish the job
        //vTaskDelay((GLOBAL_STATE->asic_job_frequency_ms - 0.3) / portTICK_PERIOD_MS);
        xSemaphoreTake(GLOBAL_STATE->ASIC_TASK_MODULE.semaphore, (GLOBAL_STATE->asic_job_frequency_ms / portTICK_PERIOD_MS));
    }
}
