#ifndef GLOBAL_STATE_H_
#define GLOBAL_STATE_H_

#include "asic_task.h"
#include "bm1366.h"
#include "bm1397.h"
#include "common.h"
#include "power_management_task.h"
#include "serial.h"
#include "stratum_api.h"
#include "system.h"
#include "work_queue.h"

#define STRATUM_USER CONFIG_STRATUM_USER

/* Use platform ID to decide which drivers to use */
#define PLATFORM_BITAXE 0x01
#define PLATFORM_ULTRA  0x02
#define PLATFORM_HEX    0x03

typedef struct
{
    void (*init_fn)(u_int64_t);
    task_result * (*receive_result_fn)(void * GLOBAL_STATE);
    int (*set_max_baud_fn)(void);
    void (*set_difficulty_mask_fn)(int);
    void (*send_work_fn)(void * GLOBAL_STATE, bm_job * next_bm_job);
} AsicFunctions;

typedef struct
{
	int platform_id;

    char * asic_model;
    AsicFunctions ASIC_functions;
    double asic_job_frequency_ms;

    work_queue stratum_queue;
    work_queue ASIC_jobs_queue;

    bm1397Module BM1397_MODULE;
    SystemModule SYSTEM_MODULE;
    AsicTaskModule ASIC_TASK_MODULE;
    PowerManagementModule POWER_MANAGEMENT_MODULE;

    char * extranonce_str;
    int extranonce_2_len;
    int abandon_work;

    uint8_t * valid_jobs;
    pthread_mutex_t valid_jobs_lock;

    uint32_t stratum_difficulty;
    uint32_t version_mask;

    int sock;

} GlobalState;

#endif /* GLOBAL_STATE_H_ */
