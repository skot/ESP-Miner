#include <string.h>

// #include "freertos/event_groups.h"
// #include "freertos/timers.h"
// #include "driver/gpio.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "i2c_bitaxe.h"
#include "DS4432U.h"
#include "EMC2101.h"
#include "INA260.h"
#include "adc.h"
#include "global_state.h"
#include "nvs_config.h"
#include "nvs_flash.h"
#include "display.h"
#include "screen.h"
#include "input.h"
#include "vcore.h"
#include "utils.h"
#include "TPS546.h"
#include "esp_psram.h"

#include "asic.h"

#define GPIO_ASIC_ENABLE CONFIG_GPIO_ASIC_ENABLE

#define TESTS_FAILED 0
#define TESTS_PASSED 1

//#define MEGA_FAIL_BUS 1 // Set to fail

/////Test Constants/////
//Test Fan Speed
#define FAN_SPEED_TARGET_MIN 1000 //RPM

//Test Core Voltage
#define CORE_VOLTAGE_TARGET_MIN 1000 //mV
#define CORE_VOLTAGE_TARGET_MAX 1300 //mV

//Test Power Consumption
#define POWER_CONSUMPTION_TARGET_SUB_402 12     //watts
#define POWER_CONSUMPTION_TARGET_402 5          //watts
#define POWER_CONSUMPTION_TARGET_GAMMA 11       //watts
#define POWER_CONSUMPTION_MARGIN 3              //+/- watts

//test hashrate
#define HASHRATE_TARGET_GAMMA 900 //GH/s
#define HASHRATE_TARGET_SUPRA 500 //GH/s
// #define HASHRATE_TARGET_ULTRA 1000 //GH/s
// #define HASHRATE_TARGET_MAX 2000 //GH/s

static const char * TAG = "self_test";

SemaphoreHandle_t BootSemaphore;

//local function prototypes
static void tests_done(GlobalState * GLOBAL_STATE, bool test_result);

bool should_test(GlobalState * GLOBAL_STATE) {
    bool is_max = GLOBAL_STATE->asic_model == ASIC_BM1397;
    uint64_t best_diff = nvs_config_get_u64(NVS_CONFIG_BEST_DIFF, 0);
    uint16_t should_self_test = nvs_config_get_u16(NVS_CONFIG_SELF_TEST, 0);
    if (should_self_test == 1 && !is_max && best_diff < 1) {
        return true;
    }
    return false;
}

static void reset_self_test() {
    ESP_LOGI(TAG, "Long press detected...");
    // Give the semaphore back
    xSemaphoreGive(BootSemaphore);
}

static void display_msg(char * msg, GlobalState * GLOBAL_STATE) 
{
    GLOBAL_STATE->SELF_TEST_MODULE.message = msg;
}

static esp_err_t test_fan_sense(GlobalState * GLOBAL_STATE)
{
    uint16_t fan_speed = 0;
    switch (GLOBAL_STATE->device_model) {
        case DEVICE_MAX:
        case DEVICE_ULTRA:
        case DEVICE_SUPRA:
        case DEVICE_GAMMA:
            fan_speed = EMC2101_get_fan_speed();
            break;
        default:
    }
    ESP_LOGI(TAG, "fanSpeed: %d", fan_speed);
    if (fan_speed > FAN_SPEED_TARGET_MIN) {
        return ESP_OK;
    }

    //fan test failed
    ESP_LOGE(TAG, "FAN test failed!");
    display_msg("FAN:WARN", GLOBAL_STATE);  
    return ESP_FAIL;
}

static esp_err_t test_INA260_power_consumption(int target_power, int margin)
{
    float power = INA260_read_power() / 1000;
    ESP_LOGI(TAG, "Power: %f", power);
    if (power > target_power -margin && power < target_power +margin) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

static esp_err_t test_TPS546_power_consumption(int target_power, int margin)
{
    float voltage = TPS546_get_vout();
    float current = TPS546_get_iout();
    float power = voltage * current;
    ESP_LOGI(TAG, "Power: %f, Voltage: %f, Current %f", power, voltage, current);
    if (power > target_power -margin && power < target_power +margin) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

static esp_err_t test_core_voltage(GlobalState * GLOBAL_STATE)
{
    uint16_t core_voltage = VCORE_get_voltage_mv(GLOBAL_STATE);
    ESP_LOGI(TAG, "Voltage: %u", core_voltage);

    if (core_voltage > CORE_VOLTAGE_TARGET_MIN && core_voltage < CORE_VOLTAGE_TARGET_MAX) {
        return ESP_OK;
    }
    //tests failed
    ESP_LOGE(TAG, "Core Voltage TEST FAIL, INCORRECT CORE VOLTAGE");
    display_msg("VCORE:FAIL", GLOBAL_STATE);
    return ESP_FAIL;
}

esp_err_t test_display(GlobalState * GLOBAL_STATE) {
    // Display testing
    switch (GLOBAL_STATE->device_model) {
        case DEVICE_MAX:
        case DEVICE_ULTRA:
        case DEVICE_SUPRA:
        case DEVICE_GAMMA:
            if (display_init(GLOBAL_STATE) != ESP_OK) {
                display_msg("DISPLAY:FAIL", GLOBAL_STATE);
                return ESP_FAIL;
            }

            if (GLOBAL_STATE->SYSTEM_MODULE.is_screen_active) {
                ESP_LOGI(TAG, "DISPLAY init success!");
            } else {
                ESP_LOGW(TAG, "DISPLAY not found!");
            }

            break;
        default:
    }

    return ESP_OK;
}

esp_err_t test_input(GlobalState * GLOBAL_STATE) {
    // Input testing
    switch (GLOBAL_STATE->device_model) {
        case DEVICE_MAX:
        case DEVICE_ULTRA:
        case DEVICE_SUPRA:
        case DEVICE_GAMMA:
            if (input_init(NULL, reset_self_test) != ESP_OK) {
                display_msg("INPUT:FAIL", GLOBAL_STATE);
                return ESP_FAIL;
            }
            
            ESP_LOGI(TAG, "INPUT init success!");
            break;
        default:
    }

    return ESP_OK;
}

esp_err_t test_screen(GlobalState * GLOBAL_STATE) {
    // Screen testing
    switch (GLOBAL_STATE->device_model) {
        case DEVICE_MAX:
        case DEVICE_ULTRA:
        case DEVICE_SUPRA:
        case DEVICE_GAMMA:
            if (screen_start(GLOBAL_STATE) != ESP_OK) {
                display_msg("SCREEN:FAIL", GLOBAL_STATE);
                return ESP_FAIL;
            }

            ESP_LOGI(TAG, "SCREEN start success!");
            
            break;
        default:
    }

    return ESP_OK;
}

esp_err_t init_voltage_regulator(GlobalState * GLOBAL_STATE) {
    ESP_RETURN_ON_ERROR(VCORE_init(GLOBAL_STATE), TAG, "VCORE init failed!");

    ESP_RETURN_ON_ERROR(VCORE_set_voltage(nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE, CONFIG_ASIC_VOLTAGE) / 1000.0, GLOBAL_STATE), TAG, "VCORE set voltage failed!");
    
    return ESP_OK;
}

esp_err_t test_voltage_regulator(GlobalState * GLOBAL_STATE) {
    
    //enable the voltage regulator GPIO on HW that supports it
    switch (GLOBAL_STATE->device_model) {
        case DEVICE_MAX:
        case DEVICE_ULTRA:
        case DEVICE_SUPRA:
            // turn ASIC on
            gpio_set_direction(GPIO_ASIC_ENABLE, GPIO_MODE_OUTPUT);
            gpio_set_level(GPIO_ASIC_ENABLE, 0);
            break;
        case DEVICE_GAMMA:
        default:
    }

    if (init_voltage_regulator(GLOBAL_STATE) != ESP_OK) {
        ESP_LOGE(TAG, "VCORE init failed!");
        display_msg("VCORE:FAIL", GLOBAL_STATE);
        //tests_done(GLOBAL_STATE, TESTS_FAILED);
        return ESP_FAIL;
    }

    // VCore regulator testing
    switch (GLOBAL_STATE->device_model) {
        case DEVICE_MAX:
        case DEVICE_ULTRA:
        case DEVICE_SUPRA:
            if (GLOBAL_STATE->board_version < 402){
                if (DS4432U_test() != ESP_OK) {
                    ESP_LOGE(TAG, "DS4432 test failed!");
                    display_msg("DS4432U:FAIL", GLOBAL_STATE);
                    //tests_done(GLOBAL_STATE, TESTS_FAILED);
                    return ESP_FAIL;
                }
            }
            break;
        case DEVICE_GAMMA:
            break;
        default:
    }

    ESP_LOGI(TAG, "Voltage Regulator test success!");
    return ESP_OK;
}

esp_err_t test_init_peripherals(GlobalState * GLOBAL_STATE) {
    
    //Init the EMC2101 fan and temperature monitoring
    switch (GLOBAL_STATE->device_model) {
        case DEVICE_MAX:
        case DEVICE_ULTRA:
        case DEVICE_SUPRA:
            ESP_RETURN_ON_ERROR(EMC2101_init(nvs_config_get_u16(NVS_CONFIG_INVERT_FAN_POLARITY, 1)), TAG, "EMC2101 init failed!");
            EMC2101_set_fan_speed(1);
            break;
        case DEVICE_GAMMA:
            ESP_RETURN_ON_ERROR(EMC2101_init(nvs_config_get_u16(NVS_CONFIG_INVERT_FAN_POLARITY, 1)), TAG, "EMC2101 init failed!");
            EMC2101_set_fan_speed(1);
            EMC2101_set_ideality_factor(EMC2101_IDEALITY_1_0319);
            EMC2101_set_beta_compensation(EMC2101_BETA_11);
            break;
        default:
    }

    //initialize the INA260, if we have one.
    switch (GLOBAL_STATE->device_model) {
        case DEVICE_MAX:
        case DEVICE_ULTRA:
        case DEVICE_SUPRA:
            if (GLOBAL_STATE->board_version < 402) {
                // Initialize the LED controller
                ESP_RETURN_ON_ERROR(INA260_init(), TAG, "INA260 init failed!");
            }
            break;
        case DEVICE_GAMMA:
        default:
    }

    ESP_LOGI(TAG, "Peripherals init success!");
    return ESP_OK;
}

esp_err_t test_psram(GlobalState * GLOBAL_STATE){
    if(!esp_psram_is_initialized()) {
        ESP_LOGE(TAG, "No PSRAM available on ESP32!");
        display_msg("PSRAM:FAIL", GLOBAL_STATE);
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * @brief Perform a self-test of the system.
 *
 * This function is intended to be run as a task and will execute a series of 
 * diagnostic tests to ensure the system is functioning correctly.
 *
 * @param pvParameters Pointer to the parameters passed to the task (if any).
 */
void self_test(void * pvParameters)
{
    GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;

    ESP_LOGI(TAG, "Running Self Tests");

    GLOBAL_STATE->SELF_TEST_MODULE.active = true;

    // Create a binary semaphore
    BootSemaphore = xSemaphoreCreateBinary();

    if (BootSemaphore == NULL) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        return;
    }

    //Run PSRAM test
    if(test_psram(GLOBAL_STATE) != ESP_OK) {
        ESP_LOGE(TAG, "NO PSRAM on device!");
        tests_done(GLOBAL_STATE, TESTS_FAILED);
    }

    //Run display tests
    if (test_display(GLOBAL_STATE) != ESP_OK) {
        ESP_LOGE(TAG, "Display test failed!");
        tests_done(GLOBAL_STATE, TESTS_FAILED);
    }

    //Run input tests
    if (test_input(GLOBAL_STATE) != ESP_OK) {
        ESP_LOGE(TAG, "Input test failed!");
        tests_done(GLOBAL_STATE, TESTS_FAILED);
    }

    //Run screen tests
    if (test_screen(GLOBAL_STATE) != ESP_OK) {
        ESP_LOGE(TAG, "Screen test failed!");
        tests_done(GLOBAL_STATE, TESTS_FAILED);
    }

    //Init peripherals EMC2101 and INA260 (if present)
    if (test_init_peripherals(GLOBAL_STATE) != ESP_OK) {
        ESP_LOGE(TAG, "Peripherals init failed!");
        tests_done(GLOBAL_STATE, TESTS_FAILED);
    }

    //Voltage Regulator Testing
    if (test_voltage_regulator(GLOBAL_STATE) != ESP_OK) {
        ESP_LOGE(TAG, "Voltage Regulator test failed!");
        tests_done(GLOBAL_STATE, TESTS_FAILED);
    }

    //test for number of ASICs
    if (SERIAL_init() != ESP_OK) {
        ESP_LOGE(TAG, "SERIAL init failed!");
        tests_done(GLOBAL_STATE, TESTS_FAILED);
    }

    uint8_t chips_detected = ASIC_init(GLOBAL_STATE);
    uint8_t chips_expected = ASIC_get_asic_count(GLOBAL_STATE);
    ESP_LOGI(TAG, "%u chips detected, %u expected", chips_detected, chips_expected);

    if (chips_detected != chips_expected) {
        ESP_LOGE(TAG, "SELF TEST FAIL, %d of %d CHIPS DETECTED", chips_detected, chips_expected);
        char error_buf[20];
        snprintf(error_buf, 20, "ASIC:FAIL %d CHIPS", chips_detected);
        display_msg(error_buf, GLOBAL_STATE);
        tests_done(GLOBAL_STATE, TESTS_FAILED);
    }

    //setup and test hashrate
    int baud = ASIC_set_max_baud(GLOBAL_STATE);
    vTaskDelay(10 / portTICK_PERIOD_MS);

    if (SERIAL_set_baud(baud) != ESP_OK) {
        ESP_LOGE(TAG, "SERIAL set baud failed!");
        tests_done(GLOBAL_STATE, TESTS_FAILED);
    }

    GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs = malloc(sizeof(bm_job *) * 128);
    GLOBAL_STATE->valid_jobs = malloc(sizeof(uint8_t) * 128);

    for (int i = 0; i < 128; i++) {
        GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[i] = NULL;
        GLOBAL_STATE->valid_jobs[i] = 0;
    }

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

    uint8_t difficulty_mask = 8;

    //(*GLOBAL_STATE->ASIC_functions.set_difficulty_mask_fn)(difficulty_mask);
    ASIC_set_job_difficulty_mask(GLOBAL_STATE, difficulty_mask);

    ESP_LOGI(TAG, "Sending work");

    //(*GLOBAL_STATE->ASIC_functions.send_work_fn)(GLOBAL_STATE, &job);
    ASIC_send_work(GLOBAL_STATE, &job);
    
     double start = esp_timer_get_time();
     double sum = 0;
     double duration = 0;
     double hash_rate = 0;

    while(duration < 3){
        task_result * asic_result = ASIC_process_work(GLOBAL_STATE);
        if (asic_result != NULL) {
            // check the nonce difficulty
            double nonce_diff = test_nonce_value(&job, asic_result->nonce, asic_result->rolled_version);
            sum += difficulty_mask;
            duration = (double) (esp_timer_get_time() - start) / 1000000;
            hash_rate = (sum * 4294967296) / (duration * 1000000000);
            ESP_LOGI(TAG, "Nonce %lu Nonce difficulty %.32f.", asic_result->nonce, nonce_diff);
            ESP_LOGI(TAG, "%f Gh/s  , duration %f",hash_rate, duration);
        }
    }

    ESP_LOGI(TAG, "Hashrate: %f", hash_rate);

    switch (GLOBAL_STATE->device_model) {
        case DEVICE_MAX:
        case DEVICE_ULTRA:
            break;
        case DEVICE_SUPRA:
            if(hash_rate < HASHRATE_TARGET_SUPRA){
                display_msg("HASHRATE:FAIL", GLOBAL_STATE);
                tests_done(GLOBAL_STATE, TESTS_FAILED);
            }
            break;
        case DEVICE_GAMMA:
            if(hash_rate < HASHRATE_TARGET_GAMMA){
                display_msg("HASHRATE:FAIL", GLOBAL_STATE);
                tests_done(GLOBAL_STATE, TESTS_FAILED);
            }
            break;
        default:
    }

    free(GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs);
    free(GLOBAL_STATE->valid_jobs);

    if (test_core_voltage(GLOBAL_STATE) != ESP_OK) {
        tests_done(GLOBAL_STATE, TESTS_FAILED);
    }

    switch (GLOBAL_STATE->device_model) {
        case DEVICE_MAX:
        case DEVICE_ULTRA:
        case DEVICE_SUPRA:
            if(GLOBAL_STATE->board_version >= 402 && GLOBAL_STATE->board_version <= 499){
                if (test_TPS546_power_consumption(POWER_CONSUMPTION_TARGET_402, POWER_CONSUMPTION_MARGIN) != ESP_OK) {
                    ESP_LOGE(TAG, "TPS546 Power Draw Failed, target %.2f", (float)POWER_CONSUMPTION_TARGET_402);
                    display_msg("POWER:FAIL", GLOBAL_STATE);
                    tests_done(GLOBAL_STATE, TESTS_FAILED);
                }
            } else {
                if (test_INA260_power_consumption(POWER_CONSUMPTION_TARGET_SUB_402, POWER_CONSUMPTION_MARGIN) != ESP_OK) {
                    ESP_LOGE(TAG, "INA260 Power Draw Failed, target %.2f", (float)POWER_CONSUMPTION_TARGET_SUB_402);
                    display_msg("POWER:FAIL", GLOBAL_STATE);
                    tests_done(GLOBAL_STATE, TESTS_FAILED);
                }
            }
            break;
        case DEVICE_GAMMA:
                if (test_TPS546_power_consumption(POWER_CONSUMPTION_TARGET_GAMMA, POWER_CONSUMPTION_MARGIN) != ESP_OK) {
                    ESP_LOGE(TAG, "TPS546 Power Draw Failed, target %.2f", (float)POWER_CONSUMPTION_TARGET_GAMMA);
                    display_msg("POWER:FAIL", GLOBAL_STATE);
                    tests_done(GLOBAL_STATE, TESTS_FAILED);
                }
            break;
        default:
    }

    if (test_fan_sense(GLOBAL_STATE) != ESP_OK) {     
        ESP_LOGE(TAG, "Fan test failed!"); 
        tests_done(GLOBAL_STATE, TESTS_FAILED);
    }

    tests_done(GLOBAL_STATE, TESTS_PASSED);

    return;  
}

static void tests_done(GlobalState * GLOBAL_STATE, bool test_result) 
{

#if MEGA_FAIL_BUS
    test_result = TESTS_FAILED;
#endif

    switch (GLOBAL_STATE->device_model) {
        case DEVICE_MAX:
        case DEVICE_ULTRA:
        case DEVICE_SUPRA:
        case DEVICE_GAMMA:
            GLOBAL_STATE->SELF_TEST_MODULE.result = test_result;
            GLOBAL_STATE->SELF_TEST_MODULE.finished = true;
            break;
        default:
    }

    if (test_result == TESTS_FAILED) {
        ESP_LOGE(TAG, "-- SELFTEST FAIL --");  
        while (1) {
            // Wait here forever until reset_self_test() gives the BootSemaphore
            if (xSemaphoreTake(BootSemaphore, portMAX_DELAY) == pdTRUE) {
                nvs_config_set_u16(NVS_CONFIG_SELF_TEST, 0);
                //wait 100ms for nvs write to finish?
                vTaskDelay(100 / portTICK_PERIOD_MS);
                esp_restart();
            }
        }
    } else {
        nvs_config_set_u16(NVS_CONFIG_SELF_TEST, 0);
        ESP_LOGI(TAG, "-- SELFTEST PASS!!! --");
    }

}
