#include "esp_err.h"
#include "global_state.h"

/* Generic bit definitions. */
#define BUTTON_BIT		    ( 0x01UL )
#define eBIT_1		        ( 0x02UL )
#define OVERHEAT_BIT		( 0x04UL )
#define eBIT_3		( 0x08UL )
#define eBIT_4		( 0x10UL )
#define eBIT_5		( 0x20UL )
#define eBIT_6		( 0x40UL )
#define eBIT_7		( 0x80UL )

#define BUTTON_BOOT GPIO_NUM_0

void DISPLAY_task(void *);
esp_err_t Display_init(void);
void Display_bad_NVS(void);
void Display_init_state(void);
void Display_mining_state(void);