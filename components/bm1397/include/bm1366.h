#ifndef BM1366_H_
#define BM1366_H_

#include "driver/gpio.h"
#include "bm1397.h"

#define BM1366_RST_PIN  GPIO_NUM_1


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
#define CRC5_MASK 0x1F

static const u_int64_t BM1366_FREQUENCY = CONFIG_BM1397_FREQUENCY;
static const u_int64_t BM1366_CORE_COUNT = 672;
static const u_int64_t BM1366_HASHRATE_S = BM1366_FREQUENCY * BM1366_CORE_COUNT * 1000000;
//2^32
//static const u_int64_t NONCE_SPACE = 4294967296;
static const double  BM1366_FULLSCAN_MS = ((double)NONCE_SPACE / (double)BM1366_HASHRATE_S) * 1000;

typedef struct {
  float frequency;
} bm1366Module;




void BM1366_init(u_int64_t frequency);

void BM1366_send_init(void);
void BM1366_send_work(job_packet *job);
void BM1366_set_job_difficulty_mask(int);
int BM1366_set_max_baud(void);
int BM1366_set_default_baud(void);
void BM1366_send_hash_frequency(float frequency);


#endif /* BM1366_H_ */