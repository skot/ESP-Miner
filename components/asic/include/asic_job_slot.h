#ifndef ASIC_JOB_SLOT_H_
#define ASIC_JOB_SLOT_H_

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "asic_job.h"

#define ASIC_JOB_SLOT_COUNT 16

typedef struct AsicTaskModule
{
    asic_job_t active_jobs[ASIC_JOB_SLOT_COUNT];
    uint8_t current_slot;
    pthread_mutex_t valid_jobs_lock;
} AsicTaskModule;

static inline uint8_t asic_encode_wire_job_slot(uint8_t job_slot, uint8_t tx_shift)
{
    return (uint8_t)(job_slot << tx_shift);
}

static inline uint8_t asic_decode_wire_job_slot(uint8_t wire_byte, uint8_t rx_shift)
{
    return (uint8_t)((wire_byte >> rx_shift) & 0x0F);
}

static inline uint8_t asic_decode_wire_sub_id(uint8_t wire_byte, uint8_t sub_id_bits)
{
    return (uint8_t)(wire_byte & ((1U << sub_id_bits) - 1));
}

void asic_init_job_slots(AsicTaskModule *task);
uint8_t asic_claim_job_slot(AsicTaskModule *task, const asic_job_t *job);
bool asic_lookup_active_job(AsicTaskModule *task, uint8_t slot, asic_job_t *out_job);
void asic_invalidate_jobs(AsicTaskModule *task);

#endif /* ASIC_JOB_SLOT_H_ */
