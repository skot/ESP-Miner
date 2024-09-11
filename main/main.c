#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/event_groups.h"

#include "main.h"
#include "i2c_master.h"
#include "asic_result_task.h"
#include "asic_task.h"
#include "create_jobs_task.h"
#include "system.h"
#include "nvs_device.h"
#include "stratum_task.h"
#include "self_test.h"
#include "network.h"
#include "display_task.h"

// Enum for display states
typedef enum {
    MAIN_STATE_INIT,
    MAIN_STATE_NET_CONNECT,
    MAIN_STATE_ASIC_INIT,
    MAIN_STATE_POOL_CONNECT,
    MAIN_STATE_MINING_INIT,
    MAIN_STATE_NORMAL,
} MainState;

// Struct for display state machine
typedef struct {
    MainState state;
} MainStateMachine;

static MainStateMachine mainStateMachine;

// Declare a variable to hold the created event group.
EventGroupHandle_t mainEventGroup;

static GlobalState GLOBAL_STATE = {.extranonce_str = NULL, .extranonce_2_len = 0, .abandon_work = 0, .version_mask = 0};

static const char * TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Welcome to the bitaxe - hack the planet!");

    //init I2C
    if (i2c_master_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init I2C");
        return;
    }

    //initialize the Bitaxe Display
    xTaskCreate(DISPLAY_task, "DISPLAY_task", 4096, (void *) &GLOBAL_STATE, 3, NULL);

    //initialize the ESP32 NVS
    if (NVSDevice_init() != ESP_OK){
        ESP_LOGE(TAG, "Failed to init NVS");
        return;
    }

    //parse the NVS config into GLOBAL_STATE
    if (NVSDevice_parse_config(&GLOBAL_STATE) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse NVS config");
        //show the error on the display
        Display_bad_NVS();
        return;
    }

    //should we run the self test?
    if (should_test(&GLOBAL_STATE)) {
        self_test((void *) &GLOBAL_STATE);
        vTaskDelay(60 * 60 * 1000 / portTICK_PERIOD_MS);
    }

    //xTaskCreate(SYSTEM_task, "SYSTEM_task", 4096, (void *) &GLOBAL_STATE, 3, NULL);

    System_init_system(&GLOBAL_STATE);

    xTaskCreate(POWER_MANAGEMENT_task, "power mangement", 8192, (void *) &GLOBAL_STATE, 10, NULL);

    // Initialize the main state machine
    mainStateMachine.state = MAIN_STATE_INIT;

    /* Attempt to create the event group. */
    mainEventGroup = xEventGroupCreate();
    EventBits_t eventBits;
    if (mainEventGroup == NULL) {
        ESP_LOGE(TAG, "Main Event group creation failed");
        return;
    }

    EventBits_t result_bits;

    while(1) {

        switch (mainStateMachine.state) {
            case MAIN_STATE_INIT:
                mainStateMachine.state = MAIN_STATE_NET_CONNECT;
                break;

            case MAIN_STATE_NET_CONNECT:
                Display_net_connect_state();
                result_bits = Network_connect(&GLOBAL_STATE);

                if (result_bits & WIFI_CONNECTED_BIT) {
                    ESP_LOGI(TAG, "Connected to SSID: %s", GLOBAL_STATE.SYSTEM_MODULE.ssid);
                    //strncpy(GLOBAL_STATE.SYSTEM_MODULE.wifi_status, "Connected!", 20);
                    mainStateMachine.state = MAIN_STATE_ASIC_INIT;
                    Network_AP_off();
                } else if (result_bits & WIFI_FAIL_BIT) {
                    ESP_LOGE(TAG, "Failed to connect to SSID: %s", GLOBAL_STATE.SYSTEM_MODULE.ssid);
                    //strncpy(GLOBAL_STATE.SYSTEM_MODULE.wifi_status, "Failed to connect", 20);
                    // User might be trying to configure with AP, just chill here
                    ESP_LOGI(TAG, "Finished, waiting for user input.");
                    //wait 1 second
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                } else {
                    ESP_LOGE(TAG, "UNEXPECTED EVENT");
                    //strncpy(GLOBAL_STATE.SYSTEM_MODULE.wifi_status, "unexpected error", 20);
                    // User might be trying to configure with AP, just chill here
                    ESP_LOGI(TAG, "Finished, waiting for user input.");
                    //wait 1 second
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                }
                break;

            case MAIN_STATE_ASIC_INIT:
                //initialize the stratum queues
                queue_init(&GLOBAL_STATE.stratum_queue);
                queue_init(&GLOBAL_STATE.ASIC_jobs_queue);

                //init serial ports and buffers for ASIC communications
                SERIAL_init();
                //call the ASIC init function (pointer)
                (*GLOBAL_STATE.ASIC_functions.init_fn)(GLOBAL_STATE.POWER_MANAGEMENT_MODULE.frequency_value, GLOBAL_STATE.asic_count);
                SERIAL_set_baud((*GLOBAL_STATE.ASIC_functions.set_max_baud_fn)());
                SERIAL_clear_buffer();

                mainStateMachine.state = MAIN_STATE_POOL_CONNECT;
                break;

            case MAIN_STATE_POOL_CONNECT:
                Display_pool_connect_state();
                xTaskCreate(stratum_task, "stratum task", 8192, (void *) &GLOBAL_STATE, 5, NULL);
                xTaskCreate(create_jobs_task, "create jobs task", 8192, (void *) &GLOBAL_STATE, 10, NULL);
                mainStateMachine.state = MAIN_STATE_MINING_INIT;
                break;

            case MAIN_STATE_MINING_INIT:
                Display_mining_init_state();
                xTaskCreate(ASIC_task, "asic task", 8192, (void *) &GLOBAL_STATE, 10, NULL);
                xTaskCreate(ASIC_result_task, "asic result task", 8192, (void *) &GLOBAL_STATE, 15, NULL);
                mainStateMachine.state = MAIN_STATE_NORMAL;
                break;

            case MAIN_STATE_NORMAL:
                //wait here for 5s or an event
                eventBits = xEventGroupWaitBits(
                        mainEventGroup,   
                        eBIT_0 | eBIT_1, //events to wait for
                        pdTRUE,        // Clear event bits before returning
                        pdFALSE,       // only require one bit to be set
                        10000 / portTICK_PERIOD_MS); // timeout

                //ESP_LOGI(TAG, "eventBits: %02X", (uint8_t)eventBits);
                break;

            default:
                break;
        }
    }
}

void MINER_set_wifi_status(wifi_status_t status, uint16_t retry_count) {
    if (status == WIFI_RETRYING) {
        snprintf(GLOBAL_STATE.SYSTEM_MODULE.wifi_status, 20, "Retrying: %d", retry_count);
        return;
    } else if (status == WIFI_CONNECT_FAILED) {
        snprintf(GLOBAL_STATE.SYSTEM_MODULE.wifi_status, 20, "Connect Failed!");
        return;
    }
}


