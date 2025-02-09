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
int calulate_hcn_from_nonce_percent(int chip_addr,int freq,float nonce_percent);
float calculate_timeout_ms(int chip_addr,int freq, int cno, float nonce_percent, float timeout_percent,int versions_per_core);
float calculate_cno(int chips) ;
int calulate_hcn_max(int chip_addr,int freq);

#endif