#ifndef MAIN_H_
#define MAIN_H_

#include "connect.h"

/* Generic bit definitions. */
#define eBIT_0		( 0x01UL )
#define eBIT_1		( 0x02UL )
#define eBIT_2		( 0x04UL )
#define eBIT_3		( 0x08UL )
#define eBIT_4		( 0x10UL )
#define eBIT_5		( 0x20UL )
#define eBIT_6		( 0x40UL )
#define eBIT_7		( 0x80UL )

// Enum for display states
typedef enum {
    MAIN_STATE_INIT,
    MAIN_STATE_NET_CONNECT,
    MAIN_STATE_ASIC_INIT,
    MAIN_STATE_POOL_CONNECT,
    MAIN_STATE_MINING_INIT,
    MAIN_STATE_NORMAL,
} main_state_t;

void MINER_set_wifi_status(wifi_status_t status, uint16_t retry_count);

#endif /* MAIN_H_ */