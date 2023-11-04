#include "bm1366.h"

#include "crc.h"
#include "global_state.h"
#include "serial.h"
#include "utils.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BM1366_RST_PIN GPIO_NUM_1

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

typedef struct __attribute__((__packed__))
{
    uint8_t preamble[2];
    uint32_t nonce;
    uint8_t midstate_num;
    uint8_t job_id;
    uint16_t version;
    uint8_t crc;
} asic_result;

static const char * TAG = "bm1366Module";

static uint8_t asic_response_buffer[CHUNK_SIZE];
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
    SERIAL_send(buf, total_length, false);

    free(buf);
}

static void _send_chain_inactive(void)
{

    unsigned char read_address[2] = {0x00, 0x00};
    // send serial data
    _send_BM1366((TYPE_CMD | GROUP_ALL | CMD_INACTIVE), read_address, 2, false);
}

static void _set_chip_address(uint8_t chipAddr)
{

    unsigned char read_address[2] = {chipAddr, 0x00};
    // send serial data
    _send_BM1366((TYPE_CMD | GROUP_SINGLE | CMD_SETADDRESS), read_address, 2, false);
}

void BM1366_send_hash_frequency(unsigned int khz)
{
    unsigned int newf = 200000;
    uint8_t cmd[] = {0x00, 0x08, 0x40, 0xA0, 0x02, 0x41};   // 200MHz fallback
    int rd, k_fd, pd1, pd2;
    uint8_t refdiv, postdiv1, postdiv2, fbdiv = 0;
    unsigned int k_pll_div, k_pll_div_lowest = 0;

    // refdiver is 2 or 1
    // postdivider 2 is 1 to 7
    // postdivider 1 is 1 to 7 and <= postdivider 2
    // fbdiv is 144 to 235

    // Find a suitable setting with the lowest pll frequency
    // If a valid setting is found with rd=2, we're not going to find a better
    // one with rd=1.
    for (rd = 2; rd > 0 && !fbdiv; rd--) {
        for (pd1 = 7; pd1 > 0; pd1--) {
            for (pd2 = 1; pd2 <= pd1; pd2++) {
                k_fd = khz * rd * pd1 * pd2 / 25;
                if (k_fd % 1000)
                    continue; // Not a round value
                if (k_fd < 160000)
                    continue; // Out of acceptable range
                if (k_fd > 235000)
                    break;    // Increasing pd2 will only give a higher fbdiv
                k_pll_div = k_fd / rd;
                if (k_pll_div_lowest && k_pll_div >= k_pll_div_lowest)
                    continue; // Not a better setting
                k_pll_div_lowest = k_pll_div;
                fbdiv = k_fd / 1000;
                refdiv = rd;
                postdiv1 = pd1;
                postdiv2 = pd2;
            }
        }
    }

    if (fbdiv == 0) {
        puts("Finding dividers failed, using default value (200000 Khz)");
    } else {
        newf = (25000 * fbdiv) / (refdiv * postdiv1 * postdiv2);
        printf("final refdiv: %d, fbdiv: %d, postdiv1: %d, postdiv2: %d, min diff value: %u\n",
            refdiv, fbdiv, postdiv1, postdiv2, k_pll_div_lowest / 1000);

        if (k_pll_div_lowest >= 96000)
            cmd[2] = 0x50;

        cmd[3] = fbdiv;
        cmd[4] = refdiv;
        cmd[5] = (postdiv1 - 1) << 4 | (postdiv2 - 1);
        ESP_LOGI(TAG, "Setting Frequency to %ukHz (%u)", khz, newf);
    }

    _send_BM1366((TYPE_CMD | GROUP_ALL | CMD_WRITE), cmd, 6, false);

}

static void do_frequency_ramp_up(unsigned int end_khz)
{
    unsigned int khz = 0, start_khz = 56250, step_khz = 6250;

    for (khz = start_khz; khz <= end_khz; khz += step_khz)
        BM1366_send_hash_frequency(khz);

    // In case end_khz is not dividable by step_khz, set explicitly
    if (khz != end_khz)
        BM1366_send_hash_frequency(end_khz);
}

static uint8_t _send_init(uint64_t frequency, uint16_t asic_count)
{

    //     //send serial data
    //     vTaskDelay(SLEEP_TIME / portTICK_PERIOD_MS);

    //     unsigned char init[6] =  {  0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF};

    //      _send_BM1366((TYPE_CMD | GROUP_ALL | CMD_WRITE), init,  6, true);

    //      unsigned char init18[7] =  { 0x55, 0xAA, 0x52, 0x05, 0x00, 0x00, 0x0A };
    //      _send_simple(init18, 7);

    //      unsigned char init2[6] =  { 0x00, 0xA8, 0x00, 0x07, 0x00, 0x00};
    //      _send_BM1366((TYPE_CMD | GROUP_ALL | CMD_WRITE), init2,  6, true);

    //      unsigned char init3[6] =  { 0x00, 0x18, 0xFF, 0x0F, 0xC1, 0x00 };
    //       _send_BM1366((TYPE_CMD | GROUP_ALL | CMD_WRITE), init3,  6, true);

    //      _send_chain_inactive();
    //      // unsigned char init4[7] =  { 0x55, 0xAA, 0x53, 0x05, 0x00, 0x00, 0x03 };
    //      // _send_simple(init4, 7);

    //      unsigned char init5[7] =  { 0x55, 0xAA, 0x40, 0x05, 0x00, 0x00, 0x1C };
    //      _send_simple(init5, 7);

    //       unsigned char init6[6] =  { 0x00, 0x3C, 0x80, 0x00, 0x85, 0x40 };
    //      _send_BM1366((TYPE_CMD | GROUP_ALL | CMD_WRITE), init6,  6, true);

    //      unsigned char init7[6] =  { 0x00, 0x3C, 0x80, 0x00, 0x80, 0x20};
    //       _send_BM1366((TYPE_CMD | GROUP_ALL | CMD_WRITE), init7,  6, true);

    //     unsigned char init17[11] =  {0x55, 0xAA, 0x51, 0x09, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF, 0x1C};
    //    _send_simple(init17, 11);

    unsigned char init0[11] = {0x55, 0xAA, 0x51, 0x09, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF, 0x1C};
    _send_simple(init0, 11);

    unsigned char init1[11] = {0x55, 0xAA, 0x51, 0x09, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF, 0x1C};
    _send_simple(init1, 11);

    unsigned char init2[11] = {0x55, 0xAA, 0x51, 0x09, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF, 0x1C};
    _send_simple(init2, 11);

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

    _send_chain_inactive();
    // unsigned char init6[7] = {0x55, 0xAA, 0x53, 0x05, 0x00, 0x00, 0x03};
    // _send_simple(init6, 7);

    // split the chip address space evenly
    uint8_t address_interval = (uint8_t) (256 / chip_counter);
    for (uint8_t i = 0; i < chip_counter; i++) {
        _set_chip_address(i * address_interval);
        // unsigned char init7[7] = { 0x55, 0xAA, 0x40, 0x05, 0x00, 0x00, 0x1C };
        // _send_simple(init7, 7);
    }

    unsigned char init135[11] = {0x55, 0xAA, 0x51, 0x09, 0x00, 0x3C, 0x80, 0x00, 0x85, 0x40, 0x0C};
    _send_simple(init135, 11);

    unsigned char init136[11] = {0x55, 0xAA, 0x51, 0x09, 0x00, 0x3C, 0x80, 0x00, 0x80, 0x20, 0x19};
    _send_simple(init136, 11);

    unsigned char init137[11] = {0x55, 0xAA, 0x51, 0x09, 0x00, 0x14, 0x00, 0x00, 0x00, 0xFF, 0x08};
    _send_simple(init137, 11);

    unsigned char init138[11] = {0x55, 0xAA, 0x51, 0x09, 0x00, 0x54, 0x00, 0x00, 0x00, 0x03, 0x1D};
    _send_simple(init138, 11);

    unsigned char init139[11] = {0x55, 0xAA, 0x51, 0x09, 0x00, 0x58, 0x02, 0x11, 0x11, 0x11, 0x06};
    _send_simple(init139, 11);

    unsigned char init171[11] = {0x55, 0xAA, 0x41, 0x09, 0x00, 0x2C, 0x00, 0x7C, 0x00, 0x03, 0x03};
    _send_simple(init171, 11);

    unsigned char init173[11] = {0x55, 0xAA, 0x51, 0x09, 0x00, 0x28, 0x11, 0x30, 0x02, 0x00, 0x03};
    _send_simple(init173, 11);

    for (uint8_t i = 0; i < chip_counter; i++) {
        unsigned char set_a8_register[6] = {i * address_interval, 0xA8, 0x00, 0x07, 0x01, 0xF0};
        _send_BM1366((TYPE_CMD | GROUP_SINGLE | CMD_WRITE), set_a8_register, 6, false);
        unsigned char set_18_register[6] = {i * address_interval, 0x18, 0xF0, 0x00, 0xC1, 0x00};
        _send_BM1366((TYPE_CMD | GROUP_SINGLE | CMD_WRITE), set_18_register, 6, false);
        unsigned char set_3c_register_first[6] = {i * address_interval, 0x3C, 0x80, 0x00, 0x85, 0x40};
        _send_BM1366((TYPE_CMD | GROUP_SINGLE | CMD_WRITE), set_3c_register_first, 6, false);
        unsigned char set_3c_register_second[6] = {i * address_interval, 0x3C, 0x80, 0x00, 0x80, 0x20};
        _send_BM1366((TYPE_CMD | GROUP_SINGLE | CMD_WRITE), set_3c_register_second, 6, false);
        unsigned char set_3c_register_third[6] = {i * address_interval, 0x3C, 0x80, 0x00, 0x82, 0xAA};
        _send_BM1366((TYPE_CMD | GROUP_SINGLE | CMD_WRITE), set_3c_register_third, 6, false);
    }

    do_frequency_ramp_up(frequency * 1000);

    unsigned char init794[11] = {0x55, 0xAA, 0x51, 0x09, 0x00, 0x10, 0x00, 0x00, 0x15, 0x1C, 0x02};
    _send_simple(init794, 11);

    unsigned char init795[11] = {0x55, 0xAA, 0x51, 0x09, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF, 0x1C};
    _send_simple(init795, 11);

    return chip_counter;
}

// reset the BM1366 via the RTS line
static void _reset(void)
{
    gpio_set_level(BM1366_RST_PIN, 0);

    // delay for 100ms
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // set the gpio pin high
    gpio_set_level(BM1366_RST_PIN, 1);

    // delay for 100ms
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

static void _send_read_address(void)
{

    unsigned char read_address[2] = {0x00, 0x00};
    // send serial data
    _send_BM1366((TYPE_CMD | GROUP_ALL | CMD_READ), read_address, 2, false);
}

uint8_t BM1366_init(uint64_t frequency, uint16_t asic_count)
{
    ESP_LOGI(TAG, "Initializing BM1366");

    memset(asic_response_buffer, 0, 1024);

    esp_rom_gpio_pad_select_gpio(BM1366_RST_PIN);
    gpio_set_direction(BM1366_RST_PIN, GPIO_MODE_OUTPUT);

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
    _send_BM1366((TYPE_CMD | GROUP_ALL | CMD_WRITE), baudrate, 6, false);
    return 115749;
}

int BM1366_set_max_baud(void)
{

    /// return 115749;

    // divider of 0 for 3,125,000
    ESP_LOGI(TAG, "Setting max baud of 1000000 ");

    unsigned char init8[11] = {0x55, 0xAA, 0x51, 0x09, 0x00, 0x28, 0x11, 0x30, 0x02, 0x00, 0x03};
    _send_simple(init8, 11);
    return 1000000;
}

void BM1366_set_job_difficulty_mask(int difficulty)
{

    return;

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

    _send_BM1366((TYPE_CMD | GROUP_ALL | CMD_WRITE), job_difficulty_mask, 6, false);
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
    // ESP_LOGI(TAG, "Added Job: %i", job.job_id);
    pthread_mutex_unlock(&GLOBAL_STATE->valid_jobs_lock);

    _send_BM1366((TYPE_JOB | GROUP_SINGLE | CMD_WRITE), (uint8_t *)&job, sizeof(BM1366_job), false);
}

asic_result * BM1366_receive_work(void)
{
    // wait for a response, wait time is pretty arbitrary
    int received = SERIAL_rx(asic_response_buffer, 11, 60000);

    if (received < 0) {
        ESP_LOGI(TAG, "Error in serial RX");
        return NULL;
    } else if (received == 0) {
        // Didn't find a solution, restart and try again
        return NULL;
    }

    if (received != 11 || asic_response_buffer[0] != 0xAA || asic_response_buffer[1] != 0x55) {
        ESP_LOGI(TAG, "Serial RX invalid %i", received);
        ESP_LOG_BUFFER_HEX(TAG, asic_response_buffer, received);
        return NULL;
    }

    return (asic_result *) asic_response_buffer;
}

static uint16_t reverse_uint16(uint16_t num)
{
    return (num >> 8) | (num << 8);
}

task_result * BM1366_proccess_work(void * pvParameters)
{

    asic_result * asic_result = BM1366_receive_work();

    if (asic_result == NULL) {
        return NULL;
    }

    uint8_t job_id = asic_result->job_id;
    uint8_t rx_job_id = job_id & 0xf8;
    ESP_LOGI(TAG, "Job ID: %02X, RX: %02X", job_id, rx_job_id);

    GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;

    if (GLOBAL_STATE->valid_jobs[rx_job_id] == 0) {
        ESP_LOGE(TAG, "Invalid job nonce found, id=%d", rx_job_id);
        return NULL;
    }

    uint32_t rolled_version = GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[rx_job_id]->version;

    // // // shift the 16 bit value left 13
    rolled_version = (reverse_uint16(asic_result->version) << 13) | rolled_version;

    result.job_id = rx_job_id;
    result.nonce = asic_result->nonce;
    result.rolled_version = rolled_version;

    return &result;
}
