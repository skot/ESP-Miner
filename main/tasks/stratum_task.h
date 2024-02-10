#ifndef STRATUM_TASK_H_
#define STRATUM_TASK_H_

typedef struct
{
    uint32_t stratum_difficulty;
} SystemTaskModule;

#ifdef __cplusplus
extern "C" {
#endif

void stratum_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif