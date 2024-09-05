#ifndef SYSTEM_H_
#define SYSTEM_H_

#include "global_state.h"

void SYSTEM_notify_accepted_share(GlobalState * GLOBAL_STATE);
void SYSTEM_notify_rejected_share(GlobalState * GLOBAL_STATE);
void SYSTEM_notify_found_nonce(GlobalState * GLOBAL_STATE, double found_diff, uint8_t job_id);
void SYSTEM_notify_mining_started(GlobalState * GLOBAL_STATE);
void SYSTEM_notify_new_ntime(GlobalState * GLOBAL_STATE, uint32_t ntime);
void SYSTEM_update_overheat_mode(GlobalState * GLOBAL_STATE);

void System_init_system(GlobalState * GLOBAL_STATE);
void System_clear_display(GlobalState * GLOBAL_STATE);
void System_init_connection(GlobalState * GLOBAL_STATE);
void System_show_ap_information(const char * error, GlobalState * GLOBAL_STATE);
void System_update_connection(GlobalState * GLOBAL_STATE);
void System_show_overheat_screen(GlobalState * GLOBAL_STATE);
void System_update_system_performance(GlobalState * GLOBAL_STATE);
void System_update_system_info(GlobalState * GLOBAL_STATE);
void System_update_esp32_info(GlobalState * GLOBAL_STATE);

#endif /* SYSTEM_H_ */
