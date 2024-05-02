#include "unity.h"
#include "serial.h"
#include "bm1366.h"
#include "common.h"
#include "DS4432U.h"
#include "EMC2101.h"
#include "INA260.h"
#include "adc.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "global_state.h"
#include "nvs_config.h"
#include "nvs_flash.h"
#include "oled.h"
#include "utils.h"

#include <string.h>

typedef struct
{
    uint8_t chip_address;
    uint32_t nonce;
    uint16_t version;
} nonce_result;

struct nonce_list {
    nonce_result *data;
    uint32_t numUnique;
    uint32_t numDuplicated;
    uint32_t size;
};

void initList(struct nonce_list *_list);
void initList(struct nonce_list *_list) {
    _list->numUnique = 0;
    _list->size = 10;
    _list->data = malloc(_list->size * sizeof(nonce_result));
}

bool isDuplicated(struct nonce_list *_list, nonce_result _item);
bool isDuplicated(struct nonce_list *_list, nonce_result _item) {
    if (_list->numUnique == _list->size) {
        _list->size += 10;
        _list->data = realloc( _list->data, _list->size * sizeof(nonce_result) );
    }
    for (uint32_t j = 0; j < _list->numUnique; j++) {
        if (_list->data[j].nonce == _item.nonce && _list->data[j].version == _item.version) {
            _list->numDuplicated++;
            return true;
        }
    }
    _list->data[_list->numUnique] = _item;
    _list->numUnique++;
    return false;
}

static GlobalState GLOBAL_STATE = {.extranonce_str = NULL, .extranonce_2_len = 0, .abandon_work = 0, .version_mask = 0};

static const char * TAG = "test_bm1366";

TEST_CASE("Testing one single BM1366 chip against a known valid block", "[bm1366]")
{
    ESP_ERROR_CHECK(nvs_flash_init());

    GLOBAL_STATE.POWER_MANAGEMENT_MODULE.frequency_value = nvs_config_get_u16(NVS_CONFIG_ASIC_FREQ, 485);
    TEST_ASSERT_LESS_OR_EQUAL_UINT(575, GLOBAL_STATE.POWER_MANAGEMENT_MODULE.frequency_value);

    GLOBAL_STATE.asic_model = nvs_config_get_string(NVS_CONFIG_ASIC_MODEL, "");
    TEST_ASSERT_EQUAL_STRING("BM1366", GLOBAL_STATE.asic_model);

    if (strcmp(GLOBAL_STATE.asic_model, "BM1366") == 0) {
        ESP_LOGI(TAG, "ASIC: BM1366");
        AsicFunctions ASIC_functions = {.init_fn = BM1366_init,
                                        .receive_result_fn = BM1366_proccess_work,
                                        .set_max_baud_fn = BM1366_set_max_baud,
                                        .set_difficulty_mask_fn = BM1366_set_job_difficulty_mask,
                                        .send_work_fn = BM1366_send_work};
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

    GLOBAL_STATE.ASIC_TASK_MODULE.active_jobs = malloc(sizeof(bm_job *) * 128);
    GLOBAL_STATE.valid_jobs = malloc(sizeof(uint8_t) * 128);
    for (int i = 0; i < 128; i++) {

        GLOBAL_STATE.ASIC_TASK_MODULE.active_jobs[i] = NULL;
        GLOBAL_STATE.valid_jobs[i] = 0;
    }

    // Init I2C
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");

    // init ADC and set DS4432U voltage
    ADC_init();
    DS4432U_set_vcore(nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE, 1200) / 1000.0);

    // init EMC2101 and set FAN speed to 100%
    EMC2101_init(nvs_config_get_u16(NVS_CONFIG_INVERT_FAN_POLARITY, 1));
    EMC2101_set_fan_speed(1);

    // turn ASIC on
    gpio_set_direction(GPIO_NUM_10, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_10, 0);

    // init serial communication
    SERIAL_init();

    uint8_t chips_detected = (*GLOBAL_STATE.ASIC_functions.init_fn)(GLOBAL_STATE.POWER_MANAGEMENT_MODULE.frequency_value);
    TEST_ASSERT_EQUAL_INT8(1, chips_detected); // to be adapted to the number of chips expected

    // set max baud rate
    SERIAL_set_baud((*GLOBAL_STATE.ASIC_functions.set_max_baud_fn)());

    ESP_LOGI(TAG, "%s is ready and waiting for work", GLOBAL_STATE.asic_model);

    /* Block #839900
        {"id":null,"method":"mining.notify","params":[
            "185abf4", //job_id
            "10439ba3ef4739860fb382d2abd355f7ee767c2400015d7e0000000000000000", //prev_block_hash
            "02000000010000000000000000000000000000000000000000000000000000000000000000ffffffff1703dcd00c5075626c69632d506f6f6c", //coinbase_1
            "ffffffff025ef88d2700000000160014979f36b9a92622b82767820f6477bdefc03e86b70000000000000000266a24aa21a9ed4b7e3a6c3bf8bff6e793b507392ae3cb0deab4c8908ce351e61d0af0ac06bdc000000000", //coinbase_2
            [
                "7e3f8209b0d3e7fcfaf4c89162636bc1756d2bc3bfcdc7a7135136ff6d7b0069",
                "e66c3e07dee92d95bb4895580fb3584f2fe1d29033818846286e23cddf9bf572",
                "5a9ff7e4b64934437af73e3fa4e5a783d9b1421de19a605b8e30edfce5d055a5",
                "d3d38299563f8a6cac7821096c6e9f8fbbe77bd063adb0687035f5cde2ff4dd0",
                "72dca2b4288e4288d1c9b79df8b88b0a24e3f85e901e99eeb62014288651ef26",
                "b417f18e7774cb36e5009f239fe08c1407dde49a7b19cf4230b67de9a348b502",
                "01261cd686cde5b97cf989953b0a0e1630c016f187a6f6e7fc44b40c950d3571",
                "6076764edef31739c715acfe5d2a480624779998987d60520033b731da129f37",
                "a953f2b682496e2777938d01f7a6626435406236d8caa64fb8a7f2a9bd05e6ff",
                "479e0549bd963a3d7f6747bdd3f615fced29a003d959d4c740c29a49943d16cf",
                "a1714ab576e5228e10ee4b6aa032412e9c20015b52a4516842358fb28cef3c7e",
                "fcb3c52ae1f391246b6250c178040d6b7fa9725391a65315e6e9dc0c1e32e336"
            ],
            "20000000", //version
            "17034219", //target
            "66221ad7", //ntime
            false] //clear_jobs
        }
    */

    ESP_LOGI(TAG, "Preparing job");
    mining_notify notify_message;
    notify_message.job_id = 839900; //Block #839900 - see above https://blockchain.info/rawblock/000000000000000000023dfafae2b6e6b5ecf9d1365fafa075dec49625721f37 or https://bitcoinexplorer.org/block-height/839900#JSON
    notify_message.prev_block_hash = "10439ba3ef4739860fb382d2abd355f7ee767c2400015d7e0000000000000000";
    notify_message.version = 0x20000000; // from mining.notify to test version rolling
    notify_message.target = 0x17034219;
    notify_message.ntime = 0x66221BDF; // actual time of resolved block, see blockchain.info/...
    notify_message.difficulty = 1000; // can be as high as 652198270
    const uint32_t expected_nonce = 3529540887;
    const uint32_t expected_version = 0x2a966000;
    
    // actual merkle_root of block #839900, see see blockchain.info/... or https://bitcoinexplorer.org/block-height/839900#JSON
    char * merkle_root = "088083f58ddef995494fec492880da49e3463cc73dee1306dbdf6cf3af77454c";

    // The merkle_root copied from blockchain.info needs to be in reversed order. Maybe there is a better way to do this?
    uint8_t merkle_root_rev[32];
    hex2bin(merkle_root, merkle_root_rev, 32);
    reverse_bytes(merkle_root_rev, 32);
    bin2hex(merkle_root_rev, 32, merkle_root, 64);

    // construct job
    bm_job job = construct_bm_job(&notify_message, merkle_root, 0);

    /* debug code, should be removed once test is functional */
    // ESP_LOGI(TAG, "job.prev_block_hash:");
    // ESP_LOG_BUFFER_HEX(TAG,  job.prev_block_hash, 32);
    // ESP_LOGI(TAG, "job.prev_block_hash_be:");
    // ESP_LOG_BUFFER_HEX(TAG,  job.prev_block_hash_be, 32);
    // ESP_LOGI(TAG, "job.merkle_root:");
    // ESP_LOG_BUFFER_HEX(TAG,  job.merkle_root, 32);
    // ESP_LOGI(TAG, "job.merkle_root_be:");
    // ESP_LOG_BUFFER_HEX(TAG,  job.merkle_root_be, 32);

    // set difficulty mask
    (*GLOBAL_STATE.ASIC_functions.set_difficulty_mask_fn)(notify_message.difficulty);

    // uncomment the lines below to simulate 4 chips / 4 nonce ranges
    // chips_detected = 4;
    BM1366_set_nonce_mask(chips_detected);

    struct timeval begin, end;
    struct nonce_list nonceList;
    initList(&nonceList);
    task_result * asic_result = NULL;

    // Why do we need to change the chip address? see https://youtu.be/6o92HhvOc1I?t=5710 for a more detailed explanation.
    // Each chip (each chip address) has a different nonce space (see BM1366_set_nonce_mask).
    // So we need to change the chip address to simulate multible chips (and to select a nonce space by doing so).
    // This unit test is designed to test one single BM1366 chip and has an option to simulate multible chips by changing the chip address.
    for (uint16_t i = 0; i < 256; i = i + (256/chips_detected)) {

        uint8_t chip_address = i;
        
        ESP_LOGI(TAG, "Changing chip address and sending job; new chip address: 0x%02x", chip_address);
        BM1366_set_single_chip_address(chip_address);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
        SERIAL_clear_buffer();

        (*GLOBAL_STATE.ASIC_functions.send_work_fn)(&GLOBAL_STATE, &job);

        ESP_LOGI(TAG, "Waiting for result ... (might take a while)");
        gettimeofday(&begin, 0);
        asic_result = (*GLOBAL_STATE.ASIC_functions.receive_result_fn)(&GLOBAL_STATE);
        if (asic_result == NULL) { continue; }

        uint32_t version = asic_result->rolled_version;
        uint32_t nonce = asic_result->nonce;

        int counter = 1;
        while (asic_result != NULL && counter <= 1000) {

            if (asic_result->rolled_version < version || (counter > 1 && asic_result->rolled_version == version && asic_result->nonce == nonce)) {
                ESP_LOGI(TAG, "Rollover detected - Nonce %lu, Version 0x%08lx", asic_result->nonce, asic_result->rolled_version);
                break;
            }
            version = asic_result->rolled_version;
            nonce = asic_result->nonce;

            double nonce_diff = test_nonce_value(&job, asic_result->nonce, asic_result->rolled_version);
            char * result_isDuplicated = "";
            if ( isDuplicated(&nonceList, (nonce_result) {.nonce = asic_result->nonce, .version = asic_result->rolled_version, .chip_address = chip_address}) ) {
                result_isDuplicated = "(duplicate)";
            }
            ESP_LOGI(TAG, "Result[%d]: Nonce %lu (0x%08lx), Nonce difficulty %.32f. rolled-version 0x%08lx %s", counter, asic_result->nonce, asic_result->nonce, nonce_diff, asic_result->rolled_version, result_isDuplicated);

            if (asic_result->nonce == expected_nonce && asic_result->rolled_version == expected_version) {
                ESP_LOGI(TAG, "Expected nonce and version match. Solution found!");
                break;
            }

            asic_result = (*GLOBAL_STATE.ASIC_functions.receive_result_fn)(&GLOBAL_STATE); // wait for next result
            counter++;
        }
        gettimeofday(&end, 0);
        ESP_LOGI(TAG, "Elapsed: %f seconds", (double)(end.tv_sec - begin.tv_sec)+(end.tv_usec - begin.tv_usec)*1e-6 );
        ESP_LOGI(TAG, "----------------------------------");

        if (asic_result != NULL && asic_result->nonce == expected_nonce) {
            break;
        }
    }
    ESP_LOGI(TAG, "%lu unique nonces, %lu duplicates.", nonceList.numUnique, nonceList.numDuplicated);
    ESP_LOGI(TAG, "----------------------------------");

    free(GLOBAL_STATE.ASIC_TASK_MODULE.active_jobs);
    free(GLOBAL_STATE.valid_jobs);
    free(nonceList.data);

    // turn ASIC off
    gpio_set_direction(GPIO_NUM_10, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_10, 1);

    TEST_ASSERT_NOT_NULL(asic_result);
    TEST_ASSERT_EQUAL_UINT32(expected_nonce, asic_result->nonce);
    TEST_ASSERT_EQUAL_UINT32(expected_version, asic_result->rolled_version);
}

