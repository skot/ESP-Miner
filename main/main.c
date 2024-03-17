
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

// #include "protocol_examples_common.h"
#include "main.h"

#include "DS4432U.h"
#include "EMC2101.h"
#include "INA260.h"
#include "adc.h"
#include "asic_result_task.h"
#include "asic_task.h"
#include "create_jobs_task.h"
#include "driver/i2c.h"
#include "esp_netif.h"
#include "global_state.h"
#include "http_server.h"
#include "nvs_config.h"
#include "oled.h"
#include "serial.h"
#include "stratum_task.h"
#include "user_input_task.h"
#include "utils.h"

static GlobalState GLOBAL_STATE = {.extranonce_str = NULL, .extranonce_2_len = 0, .abandon_work = 0, .version_mask = 0};

static const char * TAG = "miner";

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_LOGI(TAG, "NVS_CONFIG_ASIC_FREQ %f", (float) nvs_config_get_u16(NVS_CONFIG_ASIC_FREQ, CONFIG_ASIC_FREQUENCY));
    GLOBAL_STATE.POWER_MANAGEMENT_MODULE.frequency_value = nvs_config_get_u16(NVS_CONFIG_ASIC_FREQ, CONFIG_ASIC_FREQUENCY);

    GLOBAL_STATE.asic_model = nvs_config_get_string(NVS_CONFIG_ASIC_MODEL, "");
    if (strcmp(GLOBAL_STATE.asic_model, "BM1366") == 0) {
        ESP_LOGI(TAG, "ASIC: BM1366");
        AsicFunctions ASIC_functions = {.init_fn = BM1366_init,
                                        .receive_result_fn = BM1366_proccess_work,
                                        .set_max_baud_fn = BM1366_set_max_baud,
                                        .set_difficulty_mask_fn = BM1366_set_job_difficulty_mask,
                                        .send_work_fn = BM1366_send_work};
        GLOBAL_STATE.asic_job_frequency_ms = BM1366_FULLSCAN_MS;

        GLOBAL_STATE.ASIC_functions = ASIC_functions;
    } else if (strcmp(GLOBAL_STATE.asic_model, "BM1368") == 0) {
        ESP_LOGI(TAG, "ASIC: BM1368");
        AsicFunctions ASIC_functions = {.init_fn = BM1368_init,
                                        .receive_result_fn = BM1368_proccess_work,
                                        .set_max_baud_fn = BM1368_set_max_baud,
                                        .set_difficulty_mask_fn = BM1368_set_job_difficulty_mask,
                                        .send_work_fn = BM1368_send_work};

        uint64_t bm1368_hashrate = GLOBAL_STATE.POWER_MANAGEMENT_MODULE.frequency_value * BM1368_CORE_COUNT * 1000000;
        GLOBAL_STATE.asic_job_frequency_ms = ((double) NONCE_SPACE / (double) bm1368_hashrate) * 1000;

        GLOBAL_STATE.ASIC_functions = ASIC_functions;
    } else if (strcmp(GLOBAL_STATE.asic_model, "BM1397") == 0) {
        ESP_LOGI(TAG, "ASIC: BM1397");
        AsicFunctions ASIC_functions = {.init_fn = BM1397_init,
                                        .receive_result_fn = BM1397_proccess_work,
                                        .set_max_baud_fn = BM1397_set_max_baud,
                                        .set_difficulty_mask_fn = BM1397_set_job_difficulty_mask,
                                        .send_work_fn = BM1397_send_work};

        uint64_t bm1397_hashrate = GLOBAL_STATE.POWER_MANAGEMENT_MODULE.frequency_value * BM1397_CORE_COUNT * 1000000;
        GLOBAL_STATE.asic_job_frequency_ms = ((double) NONCE_SPACE / (double) bm1397_hashrate) * 1000;

        GLOBAL_STATE.ASIC_functions = ASIC_functions;
    } else {
        ESP_LOGI(TAG, "Invalid ASIC model");
        AsicFunctions ASIC_functions = {.init_fn = NULL,
                                        .receive_result_fn = NULL,
                                        .set_max_baud_fn = NULL,
                                        .set_difficulty_mask_fn = NULL,
                                        .send_work_fn = NULL};
        GLOBAL_STATE.ASIC_functions = ASIC_functions;
    }

    uint16_t should_self_test = nvs_config_get_u16(NVS_CONFIG_SELF_TEST, 1);
    if (should_self_test == 1) {
        self_test((void *) &GLOBAL_STATE);
    }

    xTaskCreate(SYSTEM_task, "SYSTEM_task", 4096, (void *) &GLOBAL_STATE, 3, NULL);

    ESP_LOGI(TAG, "Welcome to the bitaxe!");

    // pull the wifi credentials out of NVS
    char * wifi_ssid = nvs_config_get_string(NVS_CONFIG_WIFI_SSID, WIFI_SSID);
    char * wifi_pass = nvs_config_get_string(NVS_CONFIG_WIFI_PASS, WIFI_PASS);

    // copy the wifi ssid to the global state
    strncpy(GLOBAL_STATE.SYSTEM_MODULE.ssid, wifi_ssid, 20);

    // init and connect to wifi
    wifi_init(wifi_ssid, wifi_pass);
    start_rest_server((void *) &GLOBAL_STATE);
    EventBits_t result_bits = wifi_connect();

    if (result_bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to SSID: %s", wifi_ssid);
        strncpy(GLOBAL_STATE.SYSTEM_MODULE.wifi_status, "Connected!", 20);
    } else if (result_bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to SSID: %s", wifi_ssid);

        strncpy(GLOBAL_STATE.SYSTEM_MODULE.wifi_status, "Failed to connect", 20);
        // User might be trying to configure with AP, just chill here
        ESP_LOGI(TAG, "Finished, waiting for user input.");
        while (1) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        strncpy(GLOBAL_STATE.SYSTEM_MODULE.wifi_status, "unexpected error", 20);
        // User might be trying to configure with AP, just chill here
        ESP_LOGI(TAG, "Finished, waiting for user input.");
        while (1) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

    free(wifi_ssid);
    free(wifi_pass);

    // set the startup_done flag
    GLOBAL_STATE.SYSTEM_MODULE.startup_done = true;

    xTaskCreate(USER_INPUT_task, "user input", 8192, (void *) &GLOBAL_STATE, 5, NULL);
    xTaskCreate(POWER_MANAGEMENT_task, "power mangement", 8192, (void *) &GLOBAL_STATE, 10, NULL);

    if (GLOBAL_STATE.ASIC_functions.init_fn != NULL) {
        wifi_softap_off();

        queue_init(&GLOBAL_STATE.stratum_queue);
        queue_init(&GLOBAL_STATE.ASIC_jobs_queue);

        SERIAL_init();
        (*GLOBAL_STATE.ASIC_functions.init_fn)(GLOBAL_STATE.POWER_MANAGEMENT_MODULE.frequency_value);

        xTaskCreate(stratum_task, "stratum admin", 8192, (void *) &GLOBAL_STATE, 5, NULL);
        xTaskCreate(create_jobs_task, "stratum miner", 8192, (void *) &GLOBAL_STATE, 10, NULL);
        xTaskCreate(ASIC_task, "asic", 8192, (void *) &GLOBAL_STATE, 10, NULL);
        xTaskCreate(ASIC_result_task, "asic result", 8192, (void *) &GLOBAL_STATE, 15, NULL);
    }
}

void MINER_set_wifi_status(wifi_status_t status, uint16_t retry_count)
{
    if (status == WIFI_RETRYING) {
        snprintf(GLOBAL_STATE.SYSTEM_MODULE.wifi_status, 20, "Retrying: %d/%d", retry_count, WIFI_MAXIMUM_RETRY);
        return;
    } else if (status == WIFI_CONNECT_FAILED) {
        snprintf(GLOBAL_STATE.SYSTEM_MODULE.wifi_status, 20, "Connect Failed!");
        return;
    }
}

void self_test(void * pvParameters)
{
    bool ASIC_PASS = false;
    bool POWER_PASS = false;
    GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;
    SystemModule * module = &GLOBAL_STATE->SYSTEM_MODULE;

    GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs = malloc(sizeof(bm_job *) * 128);
    GLOBAL_STATE->valid_jobs = malloc(sizeof(uint8_t) * 128);

    for (int i = 0; i < 128; i++) {

        GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[i] = NULL;
        GLOBAL_STATE->valid_jobs[i] = 0;
    }

    // turn ASIC on
    gpio_set_direction(GPIO_NUM_10, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_10, 0);

    // Init I2C
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");

    ADC_init();
    DS4432U_set_vcore(nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE, CONFIG_ASIC_VOLTAGE) / 1000.0);

    EMC2101_init(nvs_config_get_u16(NVS_CONFIG_INVERT_FAN_POLARITY, 1));
    EMC2101_set_fan_speed(1);

    // oled
    if (!OLED_init()) {
        ESP_LOGI(TAG, "OLED init failed!");
    } else {
        ESP_LOGI(TAG, "OLED init success!");
        // clear the oled screen
        OLED_fill(0);
    }

    if (OLED_status()) {
        memset(module->oled_buf, 0, 20);
        snprintf(module->oled_buf, 20, "SELF TEST...");
        OLED_writeString(0, 0, module->oled_buf);

        memset(module->oled_buf, 0, 20);
        snprintf(module->oled_buf, 20, "ASIC:");
        OLED_writeString(0, 1, module->oled_buf);

        memset(module->oled_buf, 0, 20);
        snprintf(module->oled_buf, 20, "POWER:");
        OLED_writeString(0, 2, module->oled_buf);

        memset(module->oled_buf, 0, 20);
        snprintf(module->oled_buf, 20, "FAN:");
        OLED_writeString(0, 3, module->oled_buf);
    }

    SERIAL_init();
    uint8_t chips_detected = (GLOBAL_STATE->ASIC_functions.init_fn)(GLOBAL_STATE->POWER_MANAGEMENT_MODULE.frequency_value);
    ESP_LOGI(TAG, "%u chips detected", chips_detected);

    int baud = (*GLOBAL_STATE->ASIC_functions.set_max_baud_fn)();
    vTaskDelay(10 / portTICK_PERIOD_MS);
    SERIAL_set_baud(baud);

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    mining_notify notify_message;
    notify_message.job_id = 0;
    notify_message.prev_block_hash = "0c859545a3498373a57452fac22eb7113df2a465000543520000000000000000";
    notify_message.version = 0x20000004;
    notify_message.version_mask = 0x1fffe000;
    notify_message.target = 0x1705ae3a;
    notify_message.ntime = 0x647025b5;
    notify_message.difficulty = 1000000;

    const char * coinbase_tx = "01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff4b0389130cfab"
                               "e6d6d5cbab26a2599e92916edec"
                               "5657a94a0708ddb970f5c45b5d12905085617eff8e010000000000000031650707758de07b010000000000001cfd703"
                               "8212f736c7573682f0000000003"
                               "79ad0c2a000000001976a9147c154ed1dc59609e3d26abb2df2ea3d587cd8c4188ac00000000000000002c6a4c29525"
                               "34b424c4f434b3ae725d3994b81"
                               "1572c1f345deb98b56b465ef8e153ecbbd27fa37bf1b005161380000000000000000266a24aa21a9ed63b06a7946b19"
                               "0a3fda1d76165b25c9b883bcc66"
                               "21b040773050ee2a1bb18f1800000000";
    uint8_t merkles[13][32];
    int num_merkles = 13;

    hex2bin("2b77d9e413e8121cd7a17ff46029591051d0922bd90b2b2a38811af1cb57a2b2", merkles[0], 32);
    hex2bin("5c8874cef00f3a233939516950e160949ef327891c9090467cead995441d22c5", merkles[1], 32);
    hex2bin("2d91ff8e19ac5fa69a40081f26c5852d366d608b04d2efe0d5b65d111d0d8074", merkles[2], 32);
    hex2bin("0ae96f609ad2264112a0b2dfb65624bedbcea3b036a59c0173394bba3a74e887", merkles[3], 32);
    hex2bin("e62172e63973d69574a82828aeb5711fc5ff97946db10fc7ec32830b24df7bde", merkles[4], 32);
    hex2bin("adb49456453aab49549a9eb46bb26787fb538e0a5f656992275194c04651ec97", merkles[5], 32);
    hex2bin("a7bc56d04d2672a8683892d6c8d376c73d250a4871fdf6f57019bcc737d6d2c2", merkles[6], 32);
    hex2bin("d94eceb8182b4f418cd071e93ec2a8993a0898d4c93bc33d9302f60dbbd0ed10", merkles[7], 32);
    hex2bin("5ad7788b8c66f8f50d332b88a80077ce10e54281ca472b4ed9bbbbcb6cf99083", merkles[8], 32);
    hex2bin("9f9d784b33df1b3ed3edb4211afc0dc1909af9758c6f8267e469f5148ed04809", merkles[9], 32);
    hex2bin("48fd17affa76b23e6fb2257df30374da839d6cb264656a82e34b350722b05123", merkles[10], 32);
    hex2bin("c4f5ab01913fc186d550c1a28f3f3e9ffaca2016b961a6a751f8cca0089df924", merkles[11], 32);
    hex2bin("cff737e1d00176dd6bbfa73071adbb370f227cfb5fba186562e4060fcec877e1", merkles[12], 32);

    char * merkle_root = calculate_merkle_root_hash(coinbase_tx, merkles, num_merkles);

    bm_job job = construct_bm_job(&notify_message, merkle_root, 0x1fffe000);

    (*GLOBAL_STATE->ASIC_functions.set_difficulty_mask_fn)(32);

    ESP_LOGI(TAG, "Sending work");

    (*GLOBAL_STATE->ASIC_functions.send_work_fn)(GLOBAL_STATE, &job);
    // vTaskDelay((GLOBAL_STATE->asic_job_frequency_ms - 0.3) / portTICK_PERIOD_MS);

    // ESP_LOGI(TAG, "Receiving work");

    // task_result * asic_result = (*GLOBAL_STATE->ASIC_functions.receive_result_fn)(GLOBAL_STATE);
    // if (asic_result != NULL) {

    //     ESP_LOGI(TAG, "Received work");

    //     // check the nonce difficulty
    //     double nonce_diff = test_nonce_value(&job, asic_result->nonce, asic_result->rolled_version);
    //     ESP_LOGI(TAG, "Nonce %lu Nonce difficulty %.32f.", asic_result->nonce, nonce_diff);

        // if (asic_result->nonce == 4054974794) {
        if (chips_detected > 0) {
            ESP_LOGI(TAG, "SELF TEST PASS");
            ASIC_PASS = true;
            if (OLED_status()) {
                memset(module->oled_buf, 0, 20);
                snprintf(module->oled_buf, 20, "ASIC:      PASS");
                OLED_writeString(0, 1, module->oled_buf);
            }
        } else {
            ESP_LOGE(TAG, "SELF TEST FAIL, INCORRECT NONCE DIFF");
            if (OLED_status()) {
                memset(module->oled_buf, 0, 20);
                snprintf(module->oled_buf, 20, "ASIC:     FAIL");
                OLED_writeString(0, 1, module->oled_buf);
            }
        }
    // } else {
    //     ESP_LOGE(TAG, "SELF TEST FAIL, NO NONCE DIFF");
    //     if (OLED_status()) {
    //         memset(module->oled_buf, 0, 20);
    //         snprintf(module->oled_buf, 20, "ASIC:     FAIL");
    //         OLED_writeString(0, 1, module->oled_buf);
    //     }
    // }

    free(GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs);
    free(GLOBAL_STATE->valid_jobs);

    if (INA260_installed()) {
        float power = INA260_read_power() / 1000;
        ESP_LOGI(TAG, "Power: %f", power);
        if (power > 9 && power < 13) {
            POWER_PASS = true;
            if (OLED_status()) {
                memset(module->oled_buf, 0, 20);
                snprintf(module->oled_buf, 20, "POWER:     PASS");
                OLED_writeString(0, 2, module->oled_buf);
            }
        } else {
            if (OLED_status()) {
                memset(module->oled_buf, 0, 20);
                snprintf(module->oled_buf, 20, "POWER:     FAIL");
                OLED_writeString(0, 2, module->oled_buf);
            }
        }
    } else {
        if (OLED_status()) {
            memset(module->oled_buf, 0, 20);
            snprintf(module->oled_buf, 20, "POWER:      WARN");
            OLED_writeString(0, 2, module->oled_buf);
        }
    }

    uint16_t fan_speed = EMC2101_get_fan_speed();

    ESP_LOGI(TAG, "fanSpeed: %d", fan_speed);

    if (fan_speed > 1000) {
        memset(module->oled_buf, 0, 20);
        snprintf(module->oled_buf, 20, "FAN:       PASS");
        OLED_writeString(0, 3, module->oled_buf);
    } else {
        memset(module->oled_buf, 0, 20);
        snprintf(module->oled_buf, 20, "FAN:       WARN");
        OLED_writeString(0, 3, module->oled_buf);
    }

    if (POWER_PASS && ASIC_PASS) {
        nvs_config_set_u16(NVS_CONFIG_SELF_TEST, 0);
    }
    vTaskDelay(60 * 60 * 1000 / portTICK_PERIOD_MS);
}