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

#define EMC2103_TEMP_DIODE_FAULT 0x8000

#define EMC2103_EXTERNAL_DIODE1_IDEALITY 0x11
#define EMC2103_EXTERNAL_DIODE2_IDEALITY 0x12
#define EMC2103_EXTERNAL_DIODE1_BETA 0x14
#define EMC2103_EXTERNAL_DIODE2_BETA 0x15

#define EMC2103_CONFIGURATION1 0x20
#define EMC2103_CONFIGURATION2 0x21

#define EMC2103_PWM_CONFIG  0x2A
#define EMC2103_PWM_BASE_FREQ 0x2B

#define EMC2103_FAN_SETTING 0x40
#define EMC2103_PWM_DIVIDE  0x41
#define EMC2103_FAN_CONFIG1 0x42
#define EMC2103_FAN_CONFIG2 0x43

#define EMC2103_TACH_LSB 0x4F
#define EMC2103_TACH_MSB 0x4E

#define EMC2103_LUT_CONFIG1 0x50

#define EMC2103_PRODUCT_FEATURES 0xFC
#define EMC2103_PRODUCT_ID 0xFD
#define EMC2103_MANUFACTURER_ID 0xFE
#define EMC2103_REVISION 0xFF

//Table 6.10 Substrate Diode Ideality Factor Look-Up Table (BJT Model)
#define EMC2103_IDEALITY_0_9973	 0x10
#define EMC2103_IDEALITY_0_9986	 0x11
#define EMC2103_IDEALITY_1_0000	 0x12
#define EMC2103_IDEALITY_1_0013	 0x13
#define EMC2103_IDEALITY_1_0026	 0x14
#define EMC2103_IDEALITY_1_0039	 0x15
#define EMC2103_IDEALITY_1_0053	 0x16
#define EMC2103_IDEALITY_1_0066	 0x17

//Table 6.12 Beta Compensation Look Up Table
#define EMC2103_BETA_080         0x00
#define EMC2103_BETA_111         0x01
#define EMC2103_BETA_176         0x02
#define EMC2103_BETA_290         0x03
#define EMC2103_BETA_480         0x04
#define EMC2103_BETA_900         0x05
#define EMC2103_BETA_2330        0x06
#define EMC2103_BETA_DISABLED    0x07



void EMC2103_set_fan_speed(float);
// void EMC2103_read(void);
uint16_t EMC2103_get_fan_speed(void);
esp_err_t EMC2103_init(bool);
float EMC2103_get_external_temp(void);
float EMC2103_get_internal_temp(void);
void EMC2103_set_ideality_factor(uint8_t);
void EMC2103_set_beta_compensation(uint8_t);
#endif /* EMC2103_H_ */
