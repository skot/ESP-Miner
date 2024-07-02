#ifndef DS4432U_H_
#define DS4432U_H_

#include <stdbool.h>
#include "esp_check.h"

#define DS4432_VRFS 0.997

bool DS4432U_test(void);
esp_err_t DS4432U_set_current_code(uint8_t output, uint8_t code);
esp_err_t DS4432U_get_current_code(uint8_t output, uint8_t *code);
uint8_t DS4432U_voltage_to_reg(uint32_t vout_mv, uint32_t vnom_mv,
                               uint32_t ra_ohm, uint32_t rb_ohm,
                               int32_t ifs_na, uint32_t vfb_mv);
#endif /* DS4432U_H_ */
