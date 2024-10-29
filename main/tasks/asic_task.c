#include "system.h"
#include "work_queue.h"
#include "serial.h"
#include "bm1397.h"
#include <string.h>
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ASIC_task";

void ASIC_task(void *pvParameters)
{
    GlobalState *GLOBAL_STATE = (GlobalState *)pvParameters;

    // Initialize semaphore and job tracking arrays
    GLOBAL_STATE->ASIC_TASK_MODULE.semaphore = xSemaphoreCreateBinary();
    memset(GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs, 0, sizeof(bm_job *) * 128);
    memset(GLOBAL_STATE->valid_jobs, 0, sizeof(uint8_t) * 128);

    ESP_LOGI(TAG, "ASIC Job Interval: %.2f ms", GLOBAL_STATE->asic_job_frequency_ms);
    SYSTEM_notify_mining_started(GLOBAL_STATE);
    ESP_LOGI(TAG, "ASIC Ready!");

    while (1)
    {
        bm_job *next_bm_job = (bm_job *)queue_dequeue(&GLOBAL_STATE->ASIC_jobs_queue);
        if (!next_bm_job) {
            vTaskDelay(1);  // Yield to prevent busy waiting
            continue;
        }

        if (next_bm_job->pool_diff != GLOBAL_STATE->stratum_difficulty)
        {
            ESP_LOGI(TAG, "New pool difficulty %lu", next_bm_job->pool_diff);
            GLOBAL_STATE->stratum_difficulty = next_bm_job->pool_diff;
        }

        (*GLOBAL_STATE->ASIC_functions.send_work_fn)(GLOBAL_STATE, next_bm_job); // send the job to the ASIC

        // Adjust delay for ASIC job completion
        xSemaphoreTake(GLOBAL_STATE->ASIC_TASK_MODULE.semaphore, (GLOBAL_STATE->asic_job_frequency_ms + 1) / portTICK_PERIOD_MS);
    }
}
