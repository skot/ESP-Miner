#include "asic_job_slot.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "asic_job_slot";

void asic_init_job_slots(AsicTaskModule *task)
{
    pthread_mutex_init(&task->valid_jobs_lock, NULL);
    task->current_slot = 0;
    memset(task->active_jobs, 0, sizeof(task->active_jobs));
}

uint8_t asic_claim_job_slot(AsicTaskModule *task, const asic_job_t *job)
{
    pthread_mutex_lock(&task->valid_jobs_lock);
    task->current_slot = (task->current_slot + 1) % ASIC_JOB_SLOT_COUNT;
    uint8_t slot = task->current_slot;

    if (job != NULL) {
        task->active_jobs[slot] = *job;
    } else {
        memset(&task->active_jobs[slot], 0, sizeof(task->active_jobs[slot]));
    }
    task->active_jobs[slot].is_valid = true;
    pthread_mutex_unlock(&task->valid_jobs_lock);

    return slot;
}

bool asic_lookup_active_job(AsicTaskModule *task, uint8_t slot, asic_job_t *out_job)
{
    pthread_mutex_lock(&task->valid_jobs_lock);
    if (slot >= ASIC_JOB_SLOT_COUNT || !task->active_jobs[slot].is_valid)
    {
        pthread_mutex_unlock(&task->valid_jobs_lock);
        ESP_LOGW(TAG, "Invalid job nonce found, slot 0x%02X", slot);
        return false;
    }

    if (out_job != NULL) {
        *out_job = task->active_jobs[slot];
    }
    pthread_mutex_unlock(&task->valid_jobs_lock);

    return true;
}

void asic_invalidate_jobs(AsicTaskModule *task)
{
    pthread_mutex_lock(&task->valid_jobs_lock);
    for (size_t i = 0; i < ASIC_JOB_SLOT_COUNT; ++i) {
        task->active_jobs[i].is_valid = false;
    }
    pthread_mutex_unlock(&task->valid_jobs_lock);
}
