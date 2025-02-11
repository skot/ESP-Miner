#include "bm1366.h"

#include "crc.h"
#include "global_state.h"
#include "serial.h"
#include "utils.h"
#include "common.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef CONFIG_GPIO_ASIC_RESET
#define GPIO_ASIC_RESET CONFIG_GPIO_ASIC_RESET
#else
#define GPIO_ASIC_RESET 1
#endif

#define TYPE_JOB 0x20
#define TYPE_CMD 0x40

#define GROUP_SINGLE 0x00
#define GROUP_ALL 0x10

#define CMD_JOB 0x01

#define CMD_SETADDRESS 0x00
#define CMD_WRITE 0x01
#define CMD_READ 0x02
#define CMD_INACTIVE 0x03

#define RESPONSE_CMD 0x00
#define RESPONSE_JOB 0x80

#define SLEEP_TIME 20
#define FREQ_MULT 25.0

#define CLOCK_ORDER_CONTROL_0 0x80
#define CLOCK_ORDER_CONTROL_1 0x84
#define ORDERED_CLOCK_ENABLE 0x20
#define CORE_REGISTER_CONTROL 0x3C
#define PLL3_PARAMETER 0x68
#define FAST_UART_CONFIGURATION 0x28
#define TICKET_MASK 0x14
#define MISC_CONTROL 0x18

#define BM1366_TIMEOUT_MS 10000
#define BM1366_TIMEOUT_THRESHOLD 2
typedef struct __attribute__((__packed__))
{
    uint8_t preamble[2];
    uint32_t nonce;
    uint8_t midstate_num;
    uint8_t job_id;
    uint16_t version;
    uint8_t crc;
} asic_result;

static float current_frequency = 56.25;

static const char * TAG = "bm1366Module";

static uint8_t asic_response_buffer[SERIAL_BUF_SIZE];
static task_result result;

/// @brief
/// @param ftdi
/// @param header
/// @param data
/// @param len
static void _send_BM1366(uint8_t header, uint8_t * data, uint8_t data_len, bool debug)
{
    packet_type_t packet_type = (header & TYPE_JOB) ? JOB_PACKET : CMD_PACKET;
    uint8_t total_length = (packet_type == JOB_PACKET) ? (data_len + 6) : (data_len + 5);

    // allocate memory for buffer
    unsigned char * buf = malloc(total_length);

    // add the preamble
    buf[0] = 0x55;
    buf[1] = 0xAA;

    // add the header field
    buf[2] = header;

    // add the length field
    buf[3] = (packet_type == JOB_PACKET) ? (data_len + 4) : (data_len + 3);

    // add the data
    memcpy(buf + 4, data, data_len);

    // add the correct crc type
    if (packet_type == JOB_PACKET) {
        uint16_t crc16_total = crc16_false(buf + 2, data_len + 2);
        buf[4 + data_len] = (crc16_total >> 8) & 0xFF;
        buf[5 + data_len] = crc16_total & 0xFF;
    } else {
        buf[4 + data_len] = crc5(buf + 2, data_len + 2);
    }

    // send serial data
    SERIAL_send(buf, total_length, debug);

    free(buf);
}

static void _send_simple(uint8_t * data, uint8_t total_length)
{
    unsigned char * buf = malloc(total_length);
    memcpy(buf, data, total_length);
    SERIAL_send(buf, total_length, BM1366_SERIALTX_DEBUG);

    free(buf);
}

static void _send_chain_inactive(void)
{

    unsigned char read_address[2] = {0x00, 0x00};
    // send serial data
    _send_BM1366((TYPE_CMD | GROUP_ALL | CMD_INACTIVE), read_address, 2, BM1366_SERIALTX_DEBUG);
}

static void _set_chip_address(uint8_t chipAddr)
{

    unsigned char read_address[2] = {chipAddr, 0x00};
    // send serial data
    _send_BM1366((TYPE_CMD | GROUP_SINGLE | CMD_SETADDRESS), read_address, 2, BM1366_SERIALTX_DEBUG);
}

void BM1366_set_version_mask(uint32_t version_mask) 
{
    int versions_to_roll = version_mask >> 13;
    uint8_t version_byte0 = (versions_to_roll >> 8);
    uint8_t version_byte1 = (versions_to_roll & 0xFF); 
    uint8_t version_cmd[] = {0x00, 0xA4, 0x90, 0x00, version_byte0, version_byte1};
    _send_BM1366(TYPE_CMD | GROUP_ALL | CMD_WRITE, version_cmd, 6, BM1366_SERIALTX_DEBUG);
}

void BM1366_send_hash_frequency(float target_freq)
{
    // default 200Mhz if it fails
    unsigned char freqbuf[9] = {0x00, 0x08, 0x40, 0xA0, 0x02, 0x41}; // freqbuf - pll0_parameter
    float newf = 200.0;

    uint8_t fb_divider = 0;
    uint8_t post_divider1 = 0, post_divider2 = 0;
    uint8_t ref_divider = 0;
    float min_difference = 10;

    // refdiver is 2 or 1
    // postdivider 2 is 1 to 7
    // postdivider 1 is 1 to 7 and less than postdivider 2
    // fbdiv is 144 to 235
    for (uint8_t refdiv_loop = 2; refdiv_loop > 0 && fb_divider == 0; refdiv_loop--) {
        for (uint8_t postdiv1_loop = 7; postdiv1_loop > 0 && fb_divider == 0; postdiv1_loop--) {
            for (uint8_t postdiv2_loop = 1; postdiv2_loop < postdiv1_loop && fb_divider == 0; postdiv2_loop++) {
                int temp_fb_divider = round(((float) (postdiv1_loop * postdiv2_loop * target_freq * refdiv_loop) / 25.0));

                if (temp_fb_divider >= 144 && temp_fb_divider <= 235) {
                    float temp_freq = 25.0 * (float) temp_fb_divider / (float) (refdiv_loop * postdiv2_loop * postdiv1_loop);
                    float freq_diff = fabs(target_freq - temp_freq);

                    if (freq_diff < min_difference) {
                        fb_divider = temp_fb_divider;
                        post_divider1 = postdiv1_loop;
                        post_divider2 = postdiv2_loop;
                        ref_divider = refdiv_loop;
                        min_difference = freq_diff;
                        break;
                    }
                }
            }
        }
    }

    if (fb_divider == 0) {
        puts("Finding dividers failed, using default value (200Mhz)");
    } else {
        newf = 25.0 * (float) (fb_divider) / (float) (ref_divider * post_divider1 * post_divider2);
        //printf("final refdiv: %d, fbdiv: %d, postdiv1: %d, postdiv2: %d, min diff value: %f\n", ref_divider, fb_divider, post_divider1, post_divider2, min_difference);

        freqbuf[3] = fb_divider;
        freqbuf[4] = ref_divider;
        freqbuf[5] = (((post_divider1 - 1) & 0xf) << 4) + ((post_divider2 - 1) & 0xf);

        if (fb_divider * 25 / (float) ref_divider >= 2400) {
            freqbuf[2] = 0x50;
        }
    }

    _send_BM1366((TYPE_CMD | GROUP_ALL | CMD_WRITE), freqbuf, 6, BM1366_SERIALTX_DEBUG);

    ESP_LOGI(TAG, "Setting Frequency to %.2fMHz (%.2f)", target_freq, newf);
}

static void do_frequency_ramp_up(float target_frequency) {
    float step = 6.25;
    float current = current_frequency;
    float target = target_frequency;

    float direction = (target > current) ? step : -step;

    if (fmod(current, step) != 0) {
        float next_dividable;
        if (direction > 0) {
            next_dividable = ceil(current / step) * step;
        } else {
            next_dividable = floor(current / step) * step;
        }
        current = next_dividable;
        BM1366_send_hash_frequency(current);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    while ((direction > 0 && current < target) || (direction < 0 && current > target)) {
        float next_step = fmin(fabs(direction), fabs(target - current));
        current += direction > 0 ? next_step : -next_step;
        BM1366_send_hash_frequency(current);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    BM1366_send_hash_frequency(target);
    return;
}
void BM1366_set_hash_counting_number(int hcn) {
    uint8_t set_10_hash_counting[6] = {0x00, 0x10, 0x00, 0x00, 0x00, 0x00};
    set_10_hash_counting[2] = (hcn >> 24) & 0xFF;
    set_10_hash_counting[3] = (hcn >> 16) & 0xFF;
    set_10_hash_counting[4] = (hcn >> 8) & 0xFF;
    set_10_hash_counting[5] = hcn & 0xFF;
    _send_BM1366((TYPE_CMD | GROUP_ALL | CMD_WRITE), set_10_hash_counting, 6, BM1366_SERIALTX_DEBUG);

}

uint8_t BM1366_get_chip_address_interval(int chips) {
    return (uint8_t)(256/_largest_power_of_two(chips));
}

float BM1366_set_nonce_percent_and_get_timeout(uint64_t frequency, uint16_t chain_chip_count, int versions_to_roll, float nonce_percent, float timeout_percent) {
    nonce_percent = limit_percent(nonce_percent,1.0);

    int address_interval = BM1366_get_chip_address_interval(chain_chip_count);
    int cno_interval = 0; //not used by bm1366
    int hcn = calculate_version_rolling_hcn(BM1366_CORE_COUNT,address_interval,cno_interval,frequency,nonce_percent);
    BM1366_set_hash_counting_number(hcn);

    int versions_per_core = versions_to_roll/BM1366_MIDSTATE_ENGINES;
    float timeout_ms = calculate_timeout_ms(BM1366_CORE_COUNT,address_interval,(int)frequency,cno_interval,nonce_percent,timeout_percent,versions_per_core);
    
    ESP_LOGI(TAG, "Chip setting chips=%i freq=%i hcn=%i nonce_percent=%.3f timeout_percent=%.3f timeout=%.4f",chain_chip_count,(int)frequency,hcn,nonce_percent,timeout_percent,timeout_ms);

    return timeout_ms;
}



static uint8_t _send_init(uint64_t frequency, uint16_t asic_count)
{

    // set version mask
    for (int i = 0; i < 3; i++) {
        BM1366_set_version_mask(STRATUM_DEFAULT_VERSION_MASK);
    }

    // read register 00 on all chips
    unsigned char init3[7] = {0x55, 0xAA, 0x52, 0x05, 0x00, 0x00, 0x0A};
    _send_simple(init3, 7);

    int chip_counter = 0;
    while (true) {
        if(SERIAL_rx(asic_response_buffer, 11, 1000) > 0) {
            chip_counter++;
        } else {
            break;
        }
    }
    ESP_LOGI(TAG, "%i chip(s) detected on the chain, expected %i", chip_counter, asic_count);

    unsigned char init4[11] = {0x55, 0xAA, 0x51, 0x09, 0x00, 0xA8, 0x00, 0x07, 0x00, 0x00, 0x03};
    _send_simple(init4, 11);

    unsigned char init5[11] = {0x55, 0xAA, 0x51, 0x09, 0x00, 0x18, 0xFF, 0x0F, 0xC1, 0x00, 0x00};
    _send_simple(init5, 11);

    //{0x55, 0xAA, 0x53, 0x05, 0x00, 0x00, 0x03};
    _send_chain_inactive();

    // split the chip address space evenly
    uint8_t address_interval = BM1366_get_chip_address_interval(chip_counter);
    for (uint8_t i = 0; i < chip_counter; i++) {
        //{ 0x55, 0xAA, 0x40, 0x05, 0x00, 0x00, 0x1C };
        _set_chip_address(i * address_interval);
    }

    unsigned char init135[11] = {0x55, 0xAA, 0x51, 0x09, 0x00, 0x3C, 0x80, 0x00, 0x85, 0x40, 0x0C};
    _send_simple(init135, 11);

    unsigned char init136[11] = {0x55, 0xAA, 0x51, 0x09, 0x00, 0x3C, 0x80, 0x00, 0x80, 0x20, 0x19};
    _send_simple(init136, 11);

    //{0x55, 0xAA, 0x51, 0x09, 0x00, 0x14, 0x00, 0x00, 0x00, 0xFF, 0x08};
    BM1366_set_job_difficulty_mask(BM1366_ASIC_DIFFICULTY);

    unsigned char init138[11] = {0x55, 0xAA, 0x51, 0x09, 0x00, 0x54, 0x00, 0x00, 0x00, 0x03, 0x1D};
    _send_simple(init138, 11);

    unsigned char init139[11] = {0x55, 0xAA, 0x51, 0x09, 0x00, 0x58, 0x02, 0x11, 0x11, 0x11, 0x06};
    _send_simple(init139, 11);

    unsigned char init171[11] = {0x55, 0xAA, 0x41, 0x09, 0x00, 0x2C, 0x00, 0x7C, 0x00, 0x03, 0x03};
    _send_simple(init171, 11);

    //S19XP Dump sends baudrate change here.. we wait until later.
    // unsigned char init173[11] = {0x55, 0xAA, 0x51, 0x09, 0x00, 0x28, 0x11, 0x30, 0x02, 0x00, 0x03};
    // _send_simple(init173, 11);

    for (uint8_t i = 0; i < chip_counter; i++) {
        unsigned char set_a8_register[6] = {i * address_interval, 0xA8, 0x00, 0x07, 0x01, 0xF0};
        _send_BM1366((TYPE_CMD | GROUP_SINGLE | CMD_WRITE), set_a8_register, 6, BM1366_SERIALTX_DEBUG);
        unsigned char set_18_register[6] = {i * address_interval, 0x18, 0xF0, 0x00, 0xC1, 0x00};
        _send_BM1366((TYPE_CMD | GROUP_SINGLE | CMD_WRITE), set_18_register, 6, BM1366_SERIALTX_DEBUG);
        unsigned char set_3c_register_first[6] = {i * address_interval, 0x3C, 0x80, 0x00, 0x85, 0x40};
        _send_BM1366((TYPE_CMD | GROUP_SINGLE | CMD_WRITE), set_3c_register_first, 6, BM1366_SERIALTX_DEBUG);
        unsigned char set_3c_register_second[6] = {i * address_interval, 0x3C, 0x80, 0x00, 0x80, 0x20};
        _send_BM1366((TYPE_CMD | GROUP_SINGLE | CMD_WRITE), set_3c_register_second, 6, BM1366_SERIALTX_DEBUG);
        unsigned char set_3c_register_third[6] = {i * address_interval, 0x3C, 0x80, 0x00, 0x82, 0xAA};
        _send_BM1366((TYPE_CMD | GROUP_SINGLE | CMD_WRITE), set_3c_register_third, 6, BM1366_SERIALTX_DEBUG);
    }

    do_frequency_ramp_up((float)frequency);

    BM1366_set_nonce_percent_and_get_timeout(frequency,chip_counter,STRATUM_DEFAULT_VERSION_MASK>>13,1.0,1.0);
    BM1366_set_version_mask(STRATUM_DEFAULT_VERSION_MASK);

    return chip_counter;
}

// reset the BM1366 via the RTS line
static void _reset(void)
{
    gpio_set_level(GPIO_ASIC_RESET, 0);

    // delay for 100ms
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // set the gpio pin high
    gpio_set_level(GPIO_ASIC_RESET, 1);

    // delay for 100ms
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

// static void _send_read_address(void)
// {

//     unsigned char read_address[2] = {0x00, 0x00};
//     // send serial data
//     _send_BM1366((TYPE_CMD | GROUP_ALL | CMD_READ), read_address, 2, BM1366_SERIALTX_DEBUG);
// }

uint8_t BM1366_init(uint64_t frequency, uint16_t asic_count)
{
    ESP_LOGI(TAG, "Initializing BM1366");

    memset(asic_response_buffer, 0, SERIAL_BUF_SIZE);

    esp_rom_gpio_pad_select_gpio(GPIO_ASIC_RESET);
    gpio_set_direction(GPIO_ASIC_RESET, GPIO_MODE_OUTPUT);

    // reset the bm1366
    _reset();

    return _send_init(frequency, asic_count);
}

// Baud formula = 25M/((denominator+1)*8)
// The denominator is 5 bits found in the misc_control (bits 9-13)
int BM1366_set_default_baud(void)
{
    // default divider of 26 (11010) for 115,749
    unsigned char baudrate[9] = {0x00, MISC_CONTROL, 0x00, 0x00, 0b01111010, 0b00110001}; // baudrate - misc_control
    _send_BM1366((TYPE_CMD | GROUP_ALL | CMD_WRITE), baudrate, 6, BM1366_SERIALTX_DEBUG);
    return 115749;
}

int BM1366_set_max_baud(void)
{
    ESP_LOGI(TAG, "Setting max baud of 1000000");

    unsigned char reg28[11] = {0x55, 0xAA, 0x51, 0x09, 0x00, 0x28, 0x11, 0x30, 0x02, 0x00, 0x03};
    _send_simple(reg28, 11);
    return 1000000;
}

void BM1366_set_job_difficulty_mask(int difficulty)
{

    // Default mask of 256 diff
    unsigned char job_difficulty_mask[9] = {0x00, TICKET_MASK, 0b00000000, 0b00000000, 0b00000000, 0b11111111};

    // The mask must be a power of 2 so there are no holes
    // Correct:  {0b00000000, 0b00000000, 0b11111111, 0b11111111}
    // Incorrect: {0b00000000, 0b00000000, 0b11100111, 0b11111111}
    // (difficulty - 1) if it is a pow 2 then step down to second largest for more hashrate sampling
    difficulty = _largest_power_of_two(difficulty) - 1;

    // convert difficulty into char array
    // Ex: 256 = {0b00000000, 0b00000000, 0b00000000, 0b11111111}, {0x00, 0x00, 0x00, 0xff}
    // Ex: 512 = {0b00000000, 0b00000000, 0b00000001, 0b11111111}, {0x00, 0x00, 0x01, 0xff}
    for (int i = 0; i < 4; i++) {
        char value = (difficulty >> (8 * i)) & 0xFF;
        // The char is read in backwards to the register so we need to reverse them
        // So a mask of 512 looks like 0b00000000 00000000 00000001 1111111
        // and not 0b00000000 00000000 10000000 1111111

        job_difficulty_mask[5 - i] = _reverse_bits(value);
    }

    ESP_LOGI(TAG, "Setting job ASIC mask to %d", difficulty);

    _send_BM1366((TYPE_CMD | GROUP_ALL | CMD_WRITE), job_difficulty_mask, 6, BM1366_SERIALTX_DEBUG);
}

static uint8_t id = 0;

void BM1366_send_work(void * pvParameters, bm_job * next_bm_job)
{

    GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;

    BM1366_job job;
    id = (id + 8) % 128;
    job.job_id = id;
    job.num_midstates = 0x01;
    memcpy(&job.starting_nonce, &next_bm_job->starting_nonce, 4);
    memcpy(&job.nbits, &next_bm_job->target, 4);
    memcpy(&job.ntime, &next_bm_job->ntime, 4);
    memcpy(job.merkle_root, next_bm_job->merkle_root_be, 32);
    memcpy(job.prev_block_hash, next_bm_job->prev_block_hash_be, 32);
    memcpy(&job.version, &next_bm_job->version, 4);

    if (GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[job.job_id] != NULL) {
        free_bm_job(GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[job.job_id]);
    }

    GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[job.job_id] = next_bm_job;

    pthread_mutex_lock(&GLOBAL_STATE->valid_jobs_lock);
    GLOBAL_STATE->valid_jobs[job.job_id] = 1;
    pthread_mutex_unlock(&GLOBAL_STATE->valid_jobs_lock);

    //debug sent jobs - this can get crazy if the interval is short
    #if BM1366_DEBUG_JOBS
    ESP_LOGI(TAG, "Send Job: %02X", job.job_id);
    #endif

    _send_BM1366((TYPE_JOB | GROUP_SINGLE | CMD_WRITE), (uint8_t *)&job, sizeof(BM1366_job), BM1366_DEBUG_WORK);
}

asic_result * BM1366_receive_work(void)
{
    // wait for a response
    int received = SERIAL_rx(asic_response_buffer, 11, BM1366_TIMEOUT_MS);

    bool uart_err = received < 0;
    bool uart_timeout = received == 0;
    uint8_t asic_timeout_counter = 0;

    // handle response
    if (uart_err) {
        ESP_LOGI(TAG, "UART Error in serial RX");
        return NULL;
    } else if (uart_timeout) {
        if (asic_timeout_counter >= BM1366_TIMEOUT_THRESHOLD) {
            ESP_LOGE(TAG, "ASIC not sending data");
            asic_timeout_counter = 0;
        }
        asic_timeout_counter++;
        return NULL;
    }

    if (received != 11 || asic_response_buffer[0] != 0xAA || asic_response_buffer[1] != 0x55) {
        ESP_LOGI(TAG, "Serial RX invalid %i", received);
        ESP_LOG_BUFFER_HEX(TAG, asic_response_buffer, received);
        SERIAL_clear_buffer();
        return NULL;
    }

    return (asic_result *) asic_response_buffer;
}

static uint16_t reverse_uint16(uint16_t num)
{
    return (num >> 8) | (num << 8);
}

static uint32_t reverse_uint32(uint32_t val)
{
    return ((val >> 24) & 0xff) |      // Move byte 3 to byte 0
           ((val << 8) & 0xff0000) |   // Move byte 1 to byte 2
           ((val >> 8) & 0xff00) |     // Move byte 2 to byte 1
           ((val << 24) & 0xff000000); // Move byte 0 to byte 3
}

task_result * BM1366_proccess_work(void * pvParameters)
{

    asic_result * asic_result = BM1366_receive_work();

    if (asic_result == NULL) {
        return NULL;
    }

    uint8_t job_id = asic_result->job_id & 0xf8;
    uint8_t core_id = (uint8_t)((reverse_uint32(asic_result->nonce) >> 25) & 0x7f); // BM1366 has 112 cores, so it should be coded on 7 bits
    uint8_t small_core_id = asic_result->job_id & 0x07; // BM1366 has 8 small cores, so it should be coded on 3 bits
    uint32_t version_bits = (reverse_uint16(asic_result->version) << 13); // shift the 16 bit value left 13
    ESP_LOGI(TAG, "Job ID: %02X, Core: %d/%d, Ver: %08" PRIX32, job_id, core_id, small_core_id, version_bits);

    GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;

    if (GLOBAL_STATE->valid_jobs[job_id] == 0) {
        ESP_LOGW(TAG, "Invalid job found, 0x%02X", job_id);
        return NULL;
    }

    uint32_t rolled_version = GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[job_id]->version | version_bits;

    result.job_id = job_id;
    result.nonce = asic_result->nonce;
    result.rolled_version = rolled_version;

    return &result;
}
