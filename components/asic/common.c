#include "common.h"

# define XTAL_OSC_MHZ 25 // related to version rolling

unsigned char _reverse_bits(unsigned char num)
{
    unsigned char reversed = 0;
    int i;

    for (i = 0; i < 8; i++) {
        reversed <<= 1;      // Left shift the reversed variable by 1
        reversed |= num & 1; // Use bitwise OR to set the rightmost bit of reversed to the current bit of num
        num >>= 1;           // Right shift num by 1 to get the next bit
    }

    return reversed;
}

int _largest_power_of_two(int num)
{
    int power = 0;

    while (num > 1) {
        num = num >> 1;
        power++;
    }

    return 1 << power;
}

int calulate_hcn_max(int chip_addr,int freq) {
    return calculate_hcn(chip_addr,freq,1.0);
}

float calculate_cno(int chips) {
    float cno = (int)(256.0*256.0/(float)chips);
    return cno;
}

int calulate_hcn_from_nonce_percent(int chip_addr,int freq,int cno,float nonce_percent) {
    float size = nonce_percent*chip_addr*256*256;
    if (cno>0) size = nonce_percent*(float)cno*256.0;
    float hcn = size * XTAL_OSC_MHZ / freq;
    int hcn_int = (int)hcn;
    return hcn_int;
}

float calculate_timeout_ms(int big_cores,int chip_addr,int freq, int cno, float nonce_percent, float timeout_percent,int versions_per_core) {
    float hcn = calulate_hcn_from_nonce_percent(chip_addr,freq,cno,nonce_percent);
    int big_cores = _largest_power_of_two(big_cores);
    int size = 256/big_cores * 256 * 256 * 256
    float timeout_s = nonce_percent * size  * desired_versions/1000/1000;
    float timeout_ms =  timeout_s * 1000;
    return timeout_ms*timeout_percent;
}
