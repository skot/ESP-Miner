#ifndef BM1370_H_
#define BM1370_H_

#include "common.h"
#include "driver/gpio.h"
#include "mining.h"

#define ASIC_BM1370_JOB_FREQUENCY_MS 500

#define CRC5_MASK 0x1F
#define BM1370_ASIC_DIFFICULTY 256

#define BM1370_SERIALTX_DEBUG true
#define BM1370_SERIALRX_DEBUG false
#define BM1370_DEBUG_WORK false //causes insane amount of debug output
#define BM1370_DEBUG_JOBS false //causes insane amount of debug output

static const uint64_t BM1370_CORE_COUNT = 128;
static const uint64_t BM1370_SMALL_CORE_COUNT = 2040;
static const float BM1370_NONCE_PERCENT = BM1370_CORE_COUNT/256.0;  //maximum nonce space with 1 chip
static const float BM1370_MIDSTATE_ENGINES = 16.0;
static const int BM1370_HCN = 7861;
static const float BM1370_HCN_MAX = 430000.0;
static const float BM1370_HCN_PERCENT = (float)BM1370_HCN/BM1370_HCN_MAX;       //hcn limit effect
static const float BM1370_VERSION_PERCENT = 1.0;                    //version scan percent
static const float BM1370_FULLSCAN_PERCENT = 0.5;                   //normalised value 1 means do the maximum space, 0 dont wait before sending work

typedef struct
{
    float frequency;
} bm1370Module;

typedef struct __attribute__((__packed__))
{
    uint8_t job_id;
    uint8_t num_midstates;
    uint8_t starting_nonce[4];
    uint8_t nbits[4];
    uint8_t ntime[4];
    uint8_t merkle_root[32];
    uint8_t prev_block_hash[32];
    uint8_t version[4];
} BM1370_job;

uint8_t BM1370_init(uint64_t frequency, uint16_t asic_count);

uint8_t BM1370_send_init(void);
void BM1370_send_work(void * GLOBAL_STATE, bm_job * next_bm_job);
void BM1370_set_job_difficulty_mask(int);
void BM1370_set_version_mask(uint32_t version_mask);
int BM1370_set_max_baud(void);
int BM1370_set_default_baud(void);
void BM1370_send_hash_frequency(int, float, float);
task_result * BM1370_proccess_work(void * GLOBAL_STATE);

#endif /* BM1370_H_ */
