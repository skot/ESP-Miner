#ifndef COMMON_H_
#define COMMON_H_

#include <stdint.h>

typedef struct __attribute__((__packed__))
{
    uint8_t job_id;
    uint32_t nonce;
    uint32_t rolled_version;
} task_result;

unsigned char _reverse_bits(unsigned char num);
int _largest_power_of_two(int num);
float limit_percent(float percent,float max_percent);
float calculate_cno_interval(int chips);
int calculate_version_rolling_hcn(int big_cores, int address_interval, int cno_interval, int freq, float nonce_percent);
float calculate_timeout_ms(int big_cores,int chain_chip_count, int freq, float cno_interval, float nonce_percent, float timeout_percent,int versions_per_core);

#endif