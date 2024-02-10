#ifndef MAIN_H_
#define MAIN_H_

#include "connect.h"

#ifdef __cplusplus
extern "C" {
#endif

void MINER_set_wifi_status(wifi_status_t status, uint16_t retry_count);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H_ */