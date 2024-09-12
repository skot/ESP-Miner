#include "esp_err.h"
#include "global_state.h"

/* Generic bit definitions. */
#define UPDATE_HASHRATE		( 0x01UL )
#define UPDATE_SHARES		( 0x02UL )
#define UPDATE_BD		    ( 0x04UL )
#define UPDATE		        ( 0x08UL )
#define BUTTON_BIT          ( 0x10UL )
#define OVERHEAT_BIT		( 0x20UL )
#define eBIT_6		        ( 0x40UL )
#define EXIT		        ( 0x80UL )

#define BUTTON_BOOT GPIO_NUM_0


typedef enum {
    DISPLAY_STATE_SPLASH,
    DISPLAY_STATE_NET_CONNECT,
    DISPLAY_STATE_ASIC_INIT,
    DISPLAY_STATE_POOL_CONNECT,
    DISPLAY_STATE_NORMAL,
    DISPLAY_STATE_ERROR,
} display_state_t;

typedef enum {
    INFO_IP,
    INFO_SHARES,
    INFO_POOL,
    INFO_TEMP,
    INFO_STATUS,

    INFO_COUNT
} info_line_t;

void DISPLAY_task(void *);
esp_err_t Display_init(void);
void Display_bad_NVS(void);

void Display_normal_update(uint8_t);

void Display_change_state(display_state_t);