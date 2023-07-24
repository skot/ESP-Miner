#include "global_state.h"
#include "work_queue.h"
#include "serial.h"
#include "bm1397.h"
#include <string.h>
#include "esp_log.h"
#include "nvs_config.h"

const char * TAG = "asic_result";

void ASIC_result_task(void * pvParameters)
{

    GlobalState *GLOBAL_STATE = (GlobalState*) pvParameters;

    SERIAL_clear_buffer();
    uint32_t prev_nonce = 0;


    char * user = nvs_config_get_string(NVS_CONFIG_STRATUM_USER, STRATUM_USER);

    while(1){

        uint8_t nonce_found = 0;
        uint32_t first_nonce = 0;

        asic_result *asic_result = (*GLOBAL_STATE->ASIC_FUNCTIONS.receive_work_fn)();
        uint8_t job_id = asic_result->job_id;
        uint32_t nonce = asic_result->nonce;


        if(asic_result == NULL){
            continue;
        }

        uint8_t rx_job_id = job_id & 0xfc;
        uint8_t rx_midstate_index = job_id & 0x03;

        if (GLOBAL_STATE->valid_jobs[rx_job_id] == 0) {
            ESP_LOGI(TAG, "Invalid job nonce found, id=%d", job_id);
        }

        // ASIC may return the same nonce multiple times
        // or one that was already found
        // most of the time it behavies however
        if (nonce_found == 0) {
            first_nonce = nonce;
            nonce_found = 1;
        } else if (nonce == first_nonce) {
            // stop if we've already seen this nonce
            break;
        }

        if (nonce == prev_nonce) {
            continue;
        } else {
            prev_nonce = nonce;
        }

        // check the nonce difficulty
        double nonce_diff = test_nonce_value(
            GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[rx_job_id],
            nonce,
            rx_midstate_index
        );

        ESP_LOGI(TAG, "Nonce difficulty %.2f of %d.", nonce_diff, GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[rx_job_id]->pool_diff);

        if (nonce_diff > GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[rx_job_id]->pool_diff)
        {
            SYSTEM_notify_found_nonce(
                &GLOBAL_STATE->SYSTEM_MODULE,
                GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[rx_job_id]->pool_diff,
                nonce_diff,
                GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[rx_job_id]->target
            );

            uint32_t rolled_version = GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[rx_job_id]->version;
            for (int i = 0; i < rx_midstate_index; i++) {
                rolled_version = increment_bitmask(rolled_version, GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[rx_job_id]->version_mask);
            }

            STRATUM_V1_submit_share(
                GLOBAL_STATE->sock,
                user,
                GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[rx_job_id]->jobid,
                GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[rx_job_id]->extranonce2,
                GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[rx_job_id]->ntime,
                nonce,
                rolled_version ^ GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[rx_job_id]->version
            );
        }

    }

}