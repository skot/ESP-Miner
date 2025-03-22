#ifndef TPS546_H_
#define TPS546_H_

#include <stdint.h>
#include <esp_err.h>

#define TPS546_I2CADDR         0x24  //< TPS546 i2c address
#define TPS546_MANUFACTURER_ID 0xFE  //< Manufacturer ID
#define TPS546_REVISION        0xFF  //< Chip revision

/*-------------------------*/
/* These are the inital values for the voltage regulator configuration */
/* when the config revision stored in the TPS546 doesn't match, these values are used */


#define TPS546_INIT_ON_OFF_CONFIG 0x18 /* use ON_OFF command to control power */
#define OPERATION_OFF 0x00
#define OPERATION_ON  0x80

#define TPS546_INIT_PHASE 0x00  /* phase */

#define TPS546_INIT_FREQUENCY 650  /* KHz */

typedef struct
{
  /* vin voltage */
  float TPS546_INIT_VIN_ON;  /* V */
  float TPS546_INIT_VIN_OFF; /* V */
  float TPS546_INIT_VIN_UV_WARN_LIMIT; /* V */
  float TPS546_INIT_VIN_OV_FAULT_LIMIT; /* V */
  /* vout voltage */
  float TPS546_INIT_SCALE_LOOP; /* Voltage Scale factor */
  float TPS546_INIT_VOUT_MIN; /* V */
  float TPS546_INIT_VOUT_MAX; /* V */
  float TPS546_INIT_VOUT_COMMAND;  /* V absolute value */
  /* iout current */
  float TPS546_INIT_IOUT_OC_WARN_LIMIT; /* A */
  float TPS546_INIT_IOUT_OC_FAULT_LIMIT; /* A */
} TPS546_CONFIG;

/* vin voltage */
// #define TPS546_INIT_VIN_ON  11.0  /* V */
// #define TPS546_INIT_VIN_OFF 10.5  /* V */
// #define TPS546_INIT_VIN_UV_WARN_LIMIT 14.0 /* V */
// #define TPS546_INIT_VIN_OV_FAULT_LIMIT 15.0 /* V */
#define TPS546_INIT_VIN_OV_FAULT_RESPONSE 0xB7  /* retry 6 times */

  /* vout voltage */
//#define TPS546_INIT_SCALE_LOOP 0.25  /* Voltage Scale factor */
//#define TPS546_INIT_VOUT_MAX 3 /* V */
#define TPS546_INIT_VOUT_OV_FAULT_LIMIT 1.25 /* %/100 above VOUT_COMMAND */
#define TPS546_INIT_VOUT_OV_WARN_LIMIT  1.1 /* %/100 above VOUT_COMMAND */
#define TPS546_INIT_VOUT_MARGIN_HIGH 1.1 /* %/100 above VOUT */
//#define TPS546_INIT_VOUT_COMMAND 1.2  /* V absolute value */
#define TPS546_INIT_VOUT_MARGIN_LOW 0.90 /* %/100 below VOUT */
#define TPS546_INIT_VOUT_UV_WARN_LIMIT 0.90  /* %/100 below VOUT_COMMAND */
#define TPS546_INIT_VOUT_UV_FAULT_LIMIT 0.75 /* %/100 below VOUT_COMMAND */
//#define TPS546_INIT_VOUT_MIN 1 /* v */

  /* iout current */
// #define TPS546_INIT_IOUT_OC_WARN_LIMIT  50.00 /* A */
// #define TPS546_INIT_IOUT_OC_FAULT_LIMIT 55.00 /* A */
#define TPS546_INIT_IOUT_OC_FAULT_RESPONSE 0xC0  /* shut down, no retries */

  /* temperature */
// It is better to set the temperature warn limit for TPS546 more higher than Ultra 
#define TPS546_INIT_OT_WARN_LIMIT  105 /* degrees C */
#define TPS546_INIT_OT_FAULT_LIMIT 145 /* degrees C */
#define TPS546_INIT_OT_FAULT_RESPONSE 0xFF /* wait for cooling, and retry */

  /* timing */
#define TPS546_INIT_TON_DELAY 0
#define TPS546_INIT_TON_RISE 3
#define TPS546_INIT_TON_MAX_FAULT_LIMIT 0
#define TPS546_INIT_TON_MAX_FAULT_RESPONSE 0x3B
#define TPS546_INIT_TOFF_DELAY 0
#define TPS546_INIT_TOFF_FALL 0

#define INIT_STACK_CONFIG 0x0001 //One-Slave, 2-phase
#define INIT_SYNC_CONFIG 0x00D0 //Enable Auto Detect SYNC
#define INIT_PIN_DETECT_OVERRIDE 0xFFFF //use pin values

/*-------------------------*/

/* PMBUS_ON_OFF_CONFIG initialization values */
#define ON_OFF_CONFIG_PU 0x10   // turn on PU bit
#define ON_OFF_CONFIG_CMD 0x08  // turn on CMD bit
#define ON_OFF_CONFIG_CP 0x00   // turn off CP bit
#define ON_OFF_CONFIG_POLARITY 0x00 // turn off POLARITY bit
#define ON_OFF_CONFIG_DELAY 0x00 // turn off DELAY bit

//// STATUS_WORD Offsets
#define TPS546_STATUS_VOUT    0x8000 //bit 15
#define TPS546_STATUS_IOUT    0x4000
#define TPS546_STATUS_INPUT   0x2000
#define TPS546_STATUS_MFR     0x1000
#define TPS546_STATUS_PGOOD   0x0800
#define TPS546_STATUS_OTHER   0x0200

#define TPS546_STATUS_BUSY    0x0080
#define TPS546_STATUS_OFF     0x0040
#define TPS546_STATUS_VOUT_OV 0x0020
#define TPS546_STATUS_IOUT_OC 0x0010
#define TPS546_STATUS_VIN_UV  0x0008
#define TPS546_STATUS_TEMP    0x0004
#define TPS546_STATUS_CML     0x0002
#define TPS546_STATUS_NONE    0x0001


/* public functions */
esp_err_t TPS546_init(TPS546_CONFIG config);
void TPS546_read_mfr_info(uint8_t *);
void TPS546_set_mfr_info(void);
void TPS546_write_entire_config(void);
int TPS546_get_frequency(void);
void TPS546_set_frequency(int);
int TPS546_get_temperature(void);
float TPS546_get_vin(void);
float TPS546_get_iout(void);
float TPS546_get_vout(void);
esp_err_t TPS546_set_vout(float volts);
void TPS546_show_voltage_settings(void);
void TPS546_print_status(void);

esp_err_t TPS546_check_status(uint16_t *);
esp_err_t TPS546_parse_status(uint16_t status);
esp_err_t TPS546_clear_faults(void);

#endif /* TPS546_H_ */
