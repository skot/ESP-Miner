#include "global_state.h"

bool Network_connect(GlobalState *);
void Network_AP_off(void);
void Network_set_wifi_status(GlobalState * GLOBAL_STATE, wifi_status_t status, uint16_t retry_count);