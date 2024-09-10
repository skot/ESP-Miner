#ifndef MAIN_H_
#define MAIN_H_

#include "connect.h"

/* Generic bit definitions. */
#define eBIT_0		    ( 0x01UL )
#define eBIT_1		        ( 0x02UL )
#define eBIT_2		( 0x04UL )
#define eBIT_3		( 0x08UL )
#define eBIT_4		( 0x10UL )
#define eBIT_5		( 0x20UL )
#define eBIT_6		( 0x40UL )
#define eBIT_7		( 0x80UL )

void MINER_set_wifi_status(wifi_status_t status, uint16_t retry_count);

#endif /* MAIN_H_ */