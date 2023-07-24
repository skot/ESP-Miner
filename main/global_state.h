#ifndef GLOBAL_STATE_H_
#define GLOBAL_STATE_H_

#include "work_queue.h"
#include "bm1397.h"
#include "bm1366.h"
#include "system.h"
#include "stratum_api.h"
#include "asic_task.h"
#include "power_management_task.h"
#include "serial.h"

#define STRATUM_USER CONFIG_STRATUM_USER

typedef struct {
    void (*init)(u_int64_t);
    asic_result * (*receive_work_fn)(void);
    int (*set_max_baud)(void);
    void (*set_difficulty_mask)(int);
    void (*send_work)(job_packet *job);
} AsicFunctions;

typedef struct  {
    AsicFunctions ASIC_FUNCTIONS;

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