#ifndef CREATE_JOBS_TASK_H_
#define CREATE_JOBS_TASK_H_

#include <stdint.h>

typedef struct GlobalState GlobalState;

#define CREATE_JOBS_NOTIFY_RESET UINT32_MAX

void create_jobs_notify(GlobalState *state, uint32_t slot);
void create_jobs_reset(GlobalState *state);
void create_jobs_task(void *pvParameters);

#endif
