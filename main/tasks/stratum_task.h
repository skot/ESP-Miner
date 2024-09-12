#ifndef STRATUM_TASK_H_
#define STRATUM_TASK_H_

#include "global_state.h"
typedef struct
{
    uint32_t stratum_difficulty;
} SystemTaskModule;

void stratum_task(void *pvParameters);
esp_err_t Stratum_socket_connect(GlobalState *);

#endif