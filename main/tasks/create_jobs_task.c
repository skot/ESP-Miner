#include <sys/time.h>
#include <limits.h>
#include <inttypes.h>
#include <pthread.h>

#include "global_state.h"
#include "create_jobs_task.h"
#include "esp_log.h"
#include "mining.h"
#include "miner_job.h"
#include "string.h"
#include "esp_timer.h"

#include "asic.h"
#include "difficulty_controller.h"
#include "system.h"
#include "utils.h"

static const char *TAG = "create_jobs_task";
static pthread_mutex_t notification_lock = PTHREAD_MUTEX_INITIALIZER;

static void notify_job_task(GlobalState *state, uint32_t slot, bool invalidate)
{
    pthread_mutex_lock(&notification_lock);
    if (invalidate) state->job_invalidation_pending = true;
    if (state->create_jobs_task_handle) {
        xTaskNotify(state->create_jobs_task_handle, slot, eSetValueWithOverwrite);
    }
    pthread_mutex_unlock(&notification_lock);
}

void create_jobs_notify(GlobalState *state, uint32_t slot)
{
    // Retire old work before notifications can coalesce into a non-clean job.
    bool clean = miner_job_get_slot(slot)->clean_jobs;
    if (clean) ASIC_clear_work(state);
    notify_job_task(state, slot, clean);
}

void create_jobs_reset(GlobalState *state)
{
    ASIC_clear_work(state);
    notify_job_task(state, CREATE_JOBS_NOTIFY_RESET, true);
}

static void generate_work_from_miner_job(GlobalState *GLOBAL_STATE, const miner_job_t *job, uint64_t extranonce_2, uint32_t current_version)
{
    if (!job) return;

    asic_job_t next_job;
    uint32_t effective_version = job->version;
    if (!GLOBAL_STATE->DEVICE_CONFIG.family.asic.hardware_version_rolling && !miner_job_is_rollable(job)) {
        effective_version = current_version;
    }
    if (!mining_build_asic_job(job, extranonce_2, effective_version, &next_job)) {
        ESP_LOGE(TAG, "Failed to build ASIC job");
        return;
    }
    if (!GLOBAL_STATE->ASIC_initalized) {
        ESP_LOGW(TAG, "ASIC not initialized, skipping job send");
        return;
    }
    ASIC_send_job(GLOBAL_STATE, &next_job);
}

void create_jobs_task(void *pvParameters)
{
    GlobalState *GLOBAL_STATE = (GlobalState *)pvParameters;

    // active_jobs slots are zeroed by SYSTEM_init_system(),
    // before any task that touches them can run.

    uint32_t current_version_mask = 0;
    miner_job_t *current_work = NULL;
    bool current_work_sent = false;
    uint64_t extranonce_2 = 0;
    uint32_t current_version = 0;
    int timeout_ms = ASIC_get_asic_job_frequency_ms(GLOBAL_STATE);

    ESP_LOGI(TAG, "ASIC Job Interval: %d ms", timeout_ms);
    ESP_LOGI(TAG, "ASIC Ready!");

    while (1) {
        uint64_t start_time = esp_timer_get_time();
        uint32_t slot_notify = 0;
        TickType_t wait_ticks = (timeout_ms > 0) ? pdMS_TO_TICKS(timeout_ms) : 0;
        BaseType_t notified = xTaskNotifyWait(0, ULONG_MAX, &slot_notify, wait_ticks);
        timeout_ms -= (esp_timer_get_time() - start_time) / 1000;

        pthread_mutex_lock(&notification_lock);
        bool invalidated = GLOBAL_STATE->job_invalidation_pending;
        GLOBAL_STATE->job_invalidation_pending = false;
        if (invalidated) {
            uint32_t latest;
            if (xTaskNotifyWait(0, ULONG_MAX, &latest, 0) == pdTRUE) {
                slot_notify = latest;
                notified = pdTRUE;
            }
        }
        pthread_mutex_unlock(&notification_lock);
        if (invalidated) {
            current_work = NULL;
            current_work_sent = false;
            // Retire a send that crossed the receipt-side invalidation.
            ASIC_clear_work(GLOBAL_STATE);
        }
        if (notified == pdTRUE) {
            if (slot_notify == CREATE_JOBS_NOTIFY_RESET) {
                current_work = NULL;
                current_work_sent = false;
                if (!invalidated) ASIC_clear_work(GLOBAL_STATE);
                timeout_ms = ASIC_get_asic_job_frequency_ms(GLOBAL_STATE);
                continue;
            }
            miner_job_t *new_work = miner_job_get_slot((size_t)slot_notify);
            ESP_LOGI(TAG, "New Work Activated %s (type %d)", new_work->job_id, new_work->type);
            current_work = new_work;
            GLOBAL_STATE->active_job_slot_idx = (uint8_t)(slot_notify % MINER_JOB_POOL_SIZE);
            current_work_sent = false;
            current_version = new_work->version;

            if (new_work->version_mask != current_version_mask && GLOBAL_STATE->ASIC_initalized) {
                ESP_LOGI(TAG, "Set chip version rolls %i", (int)(new_work->version_mask >> 13));
                ASIC_set_version_mask(GLOBAL_STATE, new_work->version_mask);
                current_version_mask = new_work->version_mask;
            }

            extranonce_2 = 0;

            if (GLOBAL_STATE->ASIC_initalized) {
                difficulty_controller_update(GLOBAL_STATE);
            }

            if (!current_work->clean_jobs && !invalidated) {
                // Staged job for next cycle, let current ASIC cycle finish
                continue;
            }
        } else {
            if (current_work == NULL) {
                vTaskDelay(100 / portTICK_PERIOD_MS);
                continue;
            }
            if (!miner_job_is_rollable(current_work) && current_work_sent && GLOBAL_STATE->DEVICE_CONFIG.family.asic.hardware_version_rolling) {
                timeout_ms = ASIC_get_asic_job_frequency_ms(GLOBAL_STATE);
                continue;
            }
        }

        generate_work_from_miner_job(GLOBAL_STATE, current_work, extranonce_2, current_version);
        if (!current_work_sent) {
            SYSTEM_decode_and_apply_coinbase(GLOBAL_STATE, current_work);
        }
        current_work_sent = true;

        if (miner_job_is_rollable(current_work)) {
            extranonce_2++;
        } else if (!GLOBAL_STATE->DEVICE_CONFIG.family.asic.hardware_version_rolling) {
            // Software version rolling for ASICs without hardware version rolling (e.g. BM1397) on SV2 Standard Channel
            uint32_t mask = (current_work->version_mask != 0) ? current_work->version_mask : BIP320_VERSION_ROLLING_MASK;
            uint8_t midstates = GLOBAL_STATE->DEVICE_CONFIG.family.asic.software_midstates;
            for (int i = 0; i < midstates; i++) {
                current_version = increment_bitmask(current_version, mask);
            }
        }
        timeout_ms = ASIC_get_asic_job_frequency_ms(GLOBAL_STATE);
    }
}
