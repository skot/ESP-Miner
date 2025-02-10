#ifndef STRATUM_TASK_H_
#define STRATUM_TASK_H_

#include <pthread.h>
#include "work_queue.h"

#define MAX_STRATUM_POOLS 2

typedef enum {
    STRATUM_CONNECTING,
    STRATUM_CONNECTED,
    STRATUM_DISCONNECTED,
} connection_state_t;

typedef struct
{
    uint8_t id;

    connection_state_t state;

    uint32_t stratum_difficulty;
    uint32_t version_mask;

    bool new_stratum_version_rolling_msg;

    char * extranonce_str;
    int extranonce_2_len;

    int send_uid;
    int sock;
    char * username;
    char * password;
    char * host;
    char ip_address[20];
    uint16_t port;
    uint32_t retry_attempts;

    work_queue stratum_queue;

    uint8_t * jobs;
    pthread_mutex_t jobs_lock;
    bool abandon_work;

    StratumApiV1Message *message;
    StratumApiV1Buffer *buf;
} StratumConnection;

void stratum_task_watchdog(void * pvParameters);
void stratum_task_primary(void * pvParameters);
void stratum_task_secondary(void * pvParameters);
void stratum_close_connection(StratumConnection * connection);

#endif