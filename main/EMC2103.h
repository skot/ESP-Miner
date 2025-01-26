#ifndef EMC2103_H_
#define EMC2103_H_

#include "i2c_bitaxe.h"

#define EMC2103_I2CADDR_DEFAULT 0x2E ///< EMC2103 default i2c address

#define EMC2103_INTERNAL_TEMP_MSB 0x00
#define EMC2103_INTERNAL_TEMP_LSB 0x01
#define EMC2103_EXTERNAL_TEMP1_MSB 0x02
#define EMC2103_EXTERNAL_TEMP1_LSB 0x03
#define EMC2103_EXTERNAL_TEMP2_MSB 0x04
#define EMC2103_EXTERNAL_TEMP2_LSB 0x05

#define EMC2103_EXTERNAL_DIODE1_IDEALITY 0x11
#define EMC2103_EXTERNAL_DIODE2_IDEALITY 0x12
#define EMC2103_EXTERNAL_DIODE1_BETA 0x14
#define EMC2103_EXTERNAL_DIODE2_BETA 0x15

#define EMC2103_CONFIGURATION1 0x20
#define EMC2103_CONFIGURATION2 0x21

#define EMC2103_PWM_CONFIG 0x2A
#define EMC2103_PWM_BASE_FREQ 0x2B

#define EMC2103_FAN_SETTING 0x40

#define EMC2103_TACH_LSB 0x4F
#define EMC2103_TACH_MSB 0x4E

#define EMC2103_PRODUCT_FEATURES 0xFC
#define EMC2103_PRODUCT_ID 0xFD
#define EMC2103_MANUFACTURER_ID 0xFE
#define EMC2103_REVISION 0xFF



void EMC2103_set_fan_speed(float);
// void EMC2103_read(void);
uint16_t EMC2103_get_fan_speed(void);
esp_err_t EMC2103_init(bool);
float EMC2103_get_external_temp(void);
float EMC2103_get_internal_temp(void);
// void EMC2103_set_ideality_factor(uint8_t);
// void EMC2103_set_beta_compensation(uint8_t);
#endif /* EMC2103_H_ */
