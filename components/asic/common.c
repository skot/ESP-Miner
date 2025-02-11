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

    return 1 << (power+1);
}

float limit_percent(float percent,float max) {
    if (percent> max) percent = max;
    return percent;
}

float calculate_cno_interval(int chips) {
    // Register CNO Chip Nonce Offset
    //          
    // is a optional nonce offset for bm1370 for chips in a chain
    // It has more precision than address interval due to more bit being available
    // Note: CNO overrides the size set by address interval

    // address interval = 0x100 / _largest_power_of_two(chips) << 8 (8 bit precision)
    // cno              = 0x10000 / _largest_power_of_two(chips)    (16 bit precision)

    int cno_interval_max = 0x10000;

    // a float is kept as it will be multiplied by the chip address when sending the commands to chips
    float cno_interval = (float)(cno_interval_max/(float)chips);
    return cno_interval;
}

int calculate_version_rolling_hcn(int big_cores, int address_interval, int cno_interval, int freq, float nonce_percent) {
    // Register HCN Hash Counting Number
    //          
    // Calulates the nonce size for version rolling chips
    // It signifies to the chip when generate the next version and restart the nonce range
    // This function ensures HCN does not cause duplicates
    // Warning: HCN can cause duplicates if set too large if you decide not to use this function.
    big_cores = _largest_power_of_two(big_cores);

    uint32_t max_nonce_range = 0xffffffff;
    int chain_reserve = 0x100;
    int space_left = max_nonce_range/chain_reserve;
    float nonce_size = (float)(space_left / big_cores) * address_interval;

    // cno_interval overrides address interval calc
    if (cno_interval > 0) {
        float cno_interval_max = calculate_cno_interval(1);
        int space_left = (int)(max_nonce_range / cno_interval_max);
        nonce_size = (space_left / big_cores) * cno_interval;
    }
        
    int hcn = (int)(nonce_percent * nonce_size * (XTAL_OSC_MHZ / freq) );

    return hcn;
}

float calculate_timeout_ms(int big_cores,int address_interval, int freq, float cno_interval, float nonce_percent, float timeout_percent,int versions_per_core) {
    
    // This function calulates the timeout of a chip topology
    big_cores = _largest_power_of_two(big_cores);

    uint32_t max_nonce_range = 0xffffffff;
    int chain_reserve = 0x100;
    int space_left = max_nonce_range/chain_reserve;
    float nonce_size = (float)(space_left / big_cores) * address_interval;
    float computed_range_per_core = nonce_percent*nonce_size;

    // CNO overrides the adress interval sizing method
    if (cno_interval > 0) {
        float cno_interval_max = calculate_cno_interval(1);
        int space_left = (int)(max_nonce_range / cno_interval_max);
        float max_nonce_range = (space_left / big_cores) * cno_interval;
        computed_range_per_core = nonce_percent * max_nonce_range;
    }

    // This is the total size in parralell (versions and nonces)
    int total_nonce_version_size_per_core = versions_per_core * computed_range_per_core;

    float timeout_s = total_nonce_version_size_per_core/freq/1000/1000;
    float timeout_ms =  timeout_s * 1000;

    // Finally the timeout percent is applied
    return timeout_ms*timeout_percent;
}
