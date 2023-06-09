#include "global_state.h"
#include "work_queue.h"
#include "serial.h"
#include "bm1397.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "ASIC_task";

void ASIC_task(void * pvParameters)
{

    GlobalState *GLOBAL_STATE = (GlobalState*)pvParameters;

    uint8_t buf[CHUNK_SIZE];
    memset(buf, 0, 1024);

    uint8_t id = 0;

    int baud = BM1397_set_max_baud();
    SERIAL_set_baud(baud);

    SYSTEM_notify_mining_started(&GLOBAL_STATE->SYSTEM_MODULE);
    ESP_LOGI(TAG, "ASIC Ready!");
    while (1) {
        bm_job * next_bm_job = (bm_job *) queue_dequeue(&GLOBAL_STATE->ASIC_jobs_queue);

        if(next_bm_job->pool_diff != GLOBAL_STATE->stratum_difficulty){
            ESP_LOGI(TAG, "New difficulty %d", next_bm_job->pool_diff);
            BM1397_set_job_difficulty_mask(next_bm_job->pool_diff);
            GLOBAL_STATE->stratum_difficulty = next_bm_job->pool_diff;
        }


        struct job_packet job;
        // max job number is 128
        id = (id + 4) % 128;
        job.job_id = id;
        job.num_midstates = 1;
        memcpy(&job.starting_nonce, &next_bm_job->starting_nonce, 4);
        memcpy(&job.nbits, &next_bm_job->target, 4);
        memcpy(&job.ntime, &next_bm_job->ntime, 4);
        memcpy(&job.merkle4, next_bm_job->merkle_root + 28, 4);
        memcpy(job.midstate, next_bm_job->midstate, 32);
   

        SERIAL_clear_buffer();
        BM1397_send_work(&job); //send the job to the ASIC

        //wait for a response
        int received = SERIAL_rx(buf, 9, BM1397_FULLSCAN_MS);

        if (received < 0) {
            ESP_LOGI(TAG, "Error in serial RX");
            free_bm_job(next_bm_job);
            continue;
        } else if(received == 0){
            free_bm_job(next_bm_job);
            // Didn't find a solution, restart and try again
            continue;
        }
        
        if(received != 9 || buf[0] != 0xAA || buf[1] != 0x55){
            ESP_LOGI(TAG, "Serial RX invalid %i", received);
            ESP_LOG_BUFFER_HEX(TAG, buf, received);
            free_bm_job(next_bm_job);
            continue;
        }

        uint8_t nonce_found = 0;
        uint32_t first_nonce = 0;

        struct nonce_response nonce;
        memcpy((void *) &nonce, buf, sizeof(struct nonce_response));


        // check the nonce difficulty
        double nonce_diff = test_nonce_value(next_bm_job, nonce.nonce);

        ESP_LOGI(TAG, "Nonce difficulty %.2f of %d.", nonce_diff, next_bm_job->pool_diff);
        
        if (nonce_diff > next_bm_job->pool_diff)
        {
            SYSTEM_notify_found_nonce(&GLOBAL_STATE->SYSTEM_MODULE, next_bm_job->pool_diff);

            STRATUM_V1_submit_share(GLOBAL_STATE->sock, STRATUM_USER, next_bm_job->jobid, next_bm_job->ntime,
                            next_bm_job->extranonce2, nonce.nonce);
            
        }

        if(nonce_diff > GLOBAL_STATE->SYSTEM_MODULE.best_nonce_diff){
            SYSTEM_notify_best_nonce_diff(&GLOBAL_STATE->SYSTEM_MODULE, nonce_diff, next_bm_job->target);
        }

        free_bm_job(next_bm_job);
    }
}
