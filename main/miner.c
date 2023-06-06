#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "addr_from_stdin.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/ip4_addr.h"
#include "lwip/dns.h"
#include "stratum_api.h"
#include "mining.h"
#include "work_queue.h"
#include "utils.h"

#include "system.h"
#include "serial.h"
#include "bm1397.h"
#include <pthread.h>
#include <stdint.h>


#define PORT CONFIG_STRATUM_PORT
#define STRATUM_URL CONFIG_STRATUM_URL
#define STRATUM_USER CONFIG_STRATUM_USER
#define STRATUM_PW CONFIG_STRATUM_PW

#define STRATUM_DIFFICULTY CONFIG_STRATUM_DIFFICULTY

static const char *TAG = "miner";

TaskHandle_t sysTaskHandle = NULL;
TaskHandle_t serialTaskHandle = NULL;

static work_queue stratum_queue;
static work_queue ASIC_jobs_queue;

static char * extranonce_str = NULL;
static int extranonce_2_len = 0;

static int sock;

static int abandon_work = 0;
static uint32_t stratum_difficulty = 8192;
static bool difficulty_changed = false;

bm_job ** active_jobs;
uint8_t * valid_jobs;
pthread_mutex_t valid_jobs_lock;

static void ASIC_task(void * pvParameters)
{
    init_serial();

    init_BM1397();

    ESP_LOGI(TAG, "Job wait time:%f", BM1397_FULLSCAN_MS);

    //reset the bm1397
    reset_BM1397();

    //send the init command
    send_read_address();

    //read back response
    debug_serial_rx();

    //send the init commands
    send_init();

    uint8_t buf[CHUNK_SIZE];
    memset(buf, 0, 1024);

    uint8_t id = 0;

    active_jobs = malloc(sizeof(bm_job *) * 128);
    valid_jobs = malloc(sizeof(uint8_t) * 128);
    for (int i = 0; i < 128; i++) {
		active_jobs[i] = NULL;
        valid_jobs[i] = 0;
	}

    uint32_t prev_nonce = 0;

    
    set_max_baud();

    notify_system_mining_started();
    ESP_LOGI(TAG, "Mining!");
    while (1) {
        bm_job * next_bm_job = (bm_job *) queue_dequeue(&ASIC_jobs_queue);
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
        if (active_jobs[job.job_id] != NULL) {
            free(active_jobs[job.job_id]->jobid);
            free(active_jobs[job.job_id]->extranonce2);
            free(active_jobs[job.job_id]);
        }
        active_jobs[job.job_id] = next_bm_job;
   
        pthread_mutex_lock(&valid_jobs_lock);
        valid_jobs[job.job_id] = 1;
        pthread_mutex_unlock(&valid_jobs_lock);

        clear_serial_buffer();
        send_work(&job); //send the job to the ASIC

        //wait for a response
        int received = serial_rx(buf, 9, BM1397_FULLSCAN_MS);

        if (received < 0) {
            ESP_LOGI(TAG, "Error in serial RX");
            continue;
        } else if(received == 0){
            // Didn't find a solution, restart and try again
            continue;
        }
        
        if(received != 9 || buf[0] != 0xAA || buf[1] != 0x55){
            ESP_LOGI(TAG, "Serial RX invalid %i", received);
            ESP_LOG_BUFFER_HEX(TAG, buf, received);
            continue;
        }

        uint8_t nonce_found = 0;
        uint32_t first_nonce = 0;

        struct nonce_response nonce;
        memcpy((void *) &nonce, buf, sizeof(struct nonce_response));

        if (valid_jobs[nonce.job_id] == 0) {
            ESP_LOGI(TAG, "Invalid job nonce found");
        }

        //print_hex((uint8_t *) &nonce.nonce, 4, 4, "nonce: ");
        if (nonce_found == 0) {
            first_nonce = nonce.nonce;
            nonce_found = 1;
        } else if (nonce.nonce == first_nonce) {
            // stop if we've already seen this nonce
            break;
        }

        if (nonce.nonce == prev_nonce) {
            continue;
        } else {
            prev_nonce = nonce.nonce;
        }

        // check the nonce difficulty
        double nonce_diff = test_nonce_value(active_jobs[nonce.job_id], nonce.nonce);

        ESP_LOGI(TAG, "Nonce difficulty %.2f of %d.", nonce_diff, active_jobs[nonce.job_id]->pool_diff);
        
        if (nonce_diff > active_jobs[nonce.job_id]->pool_diff)
        {
            notify_system_found_nonce(active_jobs[nonce.job_id]->pool_diff);

            submit_share(sock, STRATUM_USER, active_jobs[nonce.job_id]->jobid, active_jobs[nonce.job_id]->ntime,
                            active_jobs[nonce.job_id]->extranonce2, nonce.nonce);
            
        }

    }
}


static void create_jobs_task(void * pvParameters)
{
    while (1) {
        char * next_notify_json_str = (char *) queue_dequeue(&stratum_queue);
        ESP_LOGI(TAG, "New Work Dequeued");
        uint32_t free_heap_size = esp_get_free_heap_size();
        ESP_LOGI(TAG, "miner heap free size: %u bytes", free_heap_size);

        mining_notify params = parse_mining_notify_message(next_notify_json_str);

        uint32_t extranonce_2 = 0;
        while (extranonce_2 < UINT_MAX && abandon_work == 0)
        {
            char * extranonce_2_str = extranonce_2_generate(extranonce_2, extranonce_2_len);

            char *coinbase_tx = construct_coinbase_tx(params.coinbase_1, params.coinbase_2, extranonce_str, extranonce_2_str);
            //ESP_LOGI(TAG, "Coinbase tx: %s", coinbase_tx);

            char *merkle_root = calculate_merkle_root_hash(coinbase_tx, (uint8_t(*)[32])params.merkle_branches, params.n_merkle_branches);
            //ESP_LOGI(TAG, "Merkle root: %s", merkle_root);

            bm_job next_job = construct_bm_job(&params, merkle_root);

            next_job.pool_diff = stratum_difficulty; //each job is tied to the _current_ difficulty

            bm_job * queued_next_job = malloc(sizeof(bm_job));
            memcpy(queued_next_job, &next_job, sizeof(bm_job));
            queued_next_job->extranonce2 = strdup(extranonce_2_str);
            queued_next_job->jobid = strdup(params.job_id);

            queue_enqueue(&ASIC_jobs_queue, queued_next_job);

            free(coinbase_tx);
            free(merkle_root);
            free(extranonce_2_str);
            extranonce_2++;
        }

        if (abandon_work == 1) {
            abandon_work = 0;
            ASIC_jobs_queue_clear(&ASIC_jobs_queue);
        }

        free_mining_notify(params);
        free(next_notify_json_str);
    }
}

ip_addr_t ip_Addr;
bool bDNSFound = false;

void dns_found_cb(const char * name, const ip_addr_t * ipaddr, void * callback_arg)
{
    ip_Addr = *ipaddr;
    bDNSFound = true;
}

static void stratum_task(void * pvParameters)
{
    initialize_stratum_buffer();
    char host_ip[20];
    int addr_family = 0;
    int ip_protocol = 0;

    //get ip address from hostname
    IP_ADDR4(&ip_Addr, 0, 0, 0, 0);
    printf("Get IP for URL: %s\n", STRATUM_URL);
    dns_gethostbyname(STRATUM_URL, &ip_Addr, dns_found_cb, NULL);
    while (!bDNSFound);

    //make IP address string from ip_Addr
    snprintf(host_ip, sizeof(host_ip), "%d.%d.%d.%d",
             ip4_addr1(&ip_Addr.u_addr.ip4),
             ip4_addr2(&ip_Addr.u_addr.ip4),
             ip4_addr3(&ip_Addr.u_addr.ip4),
             ip4_addr4(&ip_Addr.u_addr.ip4));
    printf("Connecting to: stratum+tcp://%s:%d (%s)\n", STRATUM_URL, PORT, host_ip);

    while (1) {
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(host_ip);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;

        sock = socket(addr_family, SOCK_STREAM, ip_protocol);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created, connecting to %s:%d", host_ip, PORT);

        int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in6));
        if (err != 0)
        {
            ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
            break;
        }

        uint32_t version_mask = 0;

        subscribe_to_stratum(sock, &extranonce_str, &extranonce_2_len);

        configure_version_rolling(sock);

        auth_to_stratum(sock, STRATUM_USER);

        ESP_LOGI(TAG, "Extranonce: %s", extranonce_str);
        ESP_LOGI(TAG, "Extranonce 2 length: %d", extranonce_2_len);

        suggest_difficulty(sock, STRATUM_DIFFICULTY);

        while (1)
        {
            char * line = receive_jsonrpc_line(sock);
            ESP_LOGI(TAG, "stratum rx: %s", line); //debug incoming stratum messages

            stratum_method method = parse_stratum_method(line);

            if (method == MINING_NOTIFY) {
                if ((difficulty_changed || should_abandon_work(line)) && stratum_queue.count > 0) {

                    if (difficulty_changed) {
                        ESP_LOGI(TAG, "pool diff changed, clearing queues");
                        difficulty_changed = false;
                    } else {
                        ESP_LOGI(TAG, "clean_jobs is true, clearing queues");
                    }
                    
                    abandon_work = 1;
                    queue_clear(&stratum_queue);

                    pthread_mutex_lock(&valid_jobs_lock);
                    ASIC_jobs_queue_clear(&ASIC_jobs_queue);
                    for (int i = 0; i < 128; i = i + 4) {
                        valid_jobs[i] = 0;
                    }
                    pthread_mutex_unlock(&valid_jobs_lock);
                }
                if (stratum_queue.count == QUEUE_SIZE) {
                    char * next_notify_json_str = (char *) queue_dequeue(&stratum_queue);
                    free(next_notify_json_str);
                }
                queue_enqueue(&stratum_queue, line);
            } else if (method == MINING_SET_DIFFICULTY) {
                uint32_t new_difficulty = parse_mining_set_difficulty_message(line);
                if (new_difficulty != stratum_difficulty) {
                    stratum_difficulty = new_difficulty;
                    difficulty_changed = true;
                    ESP_LOGI(TAG, "Set stratum difficulty: %d", stratum_difficulty);
                    set_job_difficulty_mask(stratum_difficulty);
                }
                free(line);

            } else if (method == MINING_SET_VERSION_MASK) {
                version_mask = parse_mining_set_version_mask_message(line);

                //1fffe000
                ESP_LOGI(TAG, "Set version mask: %08x", version_mask);
                free(line);

            } else if (method == STRATUM_RESULT) {
                int16_t parsed_id;
                if (parse_stratum_result_message(line, &parsed_id)) {
                    ESP_LOGI(TAG, "message id %d result accepted", parsed_id);
                    notify_system_accepted_share();
                } else {
                    ESP_LOGI(TAG, "message id %d result rejected", parsed_id);
                    notify_system_rejected_share();
                }
                free(line);
            }  else {
                free(line);
            }
               
            
        }

        if (sock != -1)
        {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Welcome to the bitaxe!");
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    xTaskCreate(system_task, "System_Task", 4096, NULL, 10, &sysTaskHandle);

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    queue_init(&stratum_queue);
    queue_init(&ASIC_jobs_queue);

    xTaskCreate(stratum_task, "stratum admin", 8192, NULL, 15, NULL);
    xTaskCreate(create_jobs_task, "stratum miner", 8192, NULL, 10, NULL);
    xTaskCreate(ASIC_task, "asic", 8192, NULL, 10, &serialTaskHandle);
}
