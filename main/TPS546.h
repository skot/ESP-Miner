#ifndef TPS546_H_
#define TPS546_H_

#define TPS546_I2CADDR         0x24  //< TPS546 i2c address
#define TPS546_MANUFACTURER_ID 0xFE  //< Manufacturer ID
#define TPS546_REVISION        0xFF  //< Chip revision

/*-------------------------*/
/* These are the inital values for the voltage regulator configuration */
/* when the config revision stored in the TPS546 doesn't match, these values are used */


#define TPS546_INIT_FREQUENCY 650  /* KHz */

/* vin voltage */
#define TPS546_INIT_VIN_ON  11900  /* mV */
#define TPS546_INIT_VIN_OFF 11000  /* mV */
#define TPS546_INIT_VIN_UV_WARN_LIMIT 11000 /* mV */
#define TPS546_INIT_VIN_OV_FAULT_LIMIT 14000 /* mV */
#define TPS546_INIT_VIN_OV_FAULT_RESPONSE 0xB7  /* retry 6 times */

  /* vout voltage */
#define TPS546_INIT_SCALE_LOOP 0xC808  /* 0.125 */
#define TPS546_INIT_VOUT_MAX 4500 /* mV */
#define TPS546_INIT_VOUT_OV_FAULT_LIMIT 4500 /* mV */
#define TPS546_INIT_VOUT_OV_WARN_LIMIT  4400 /* mV */
#define TPS546_INIT_VOUT_COMMAND 3600 /* mV */
#define TPS546_INIT_VOUT_UV_WARN_LIMIT 3100 /* mV */
#define TPS546_INIT_VOUT_UV_FAULT_LIMIT 3000 /* mV */
#define TPS546_INIT_VOUT_MIN 3000 /* mv */

  /* iout current */
#define TPS546_INIT_IOUT_OC_WARN_LIMIT 25000 /* mA */
#define TPS546_INIT_IOUT_OC_FAULT_LIMIT 30000 /* mA */
#define TPS546_INIT_IOUT_OC_FAULT_RESPONSE 0xC0  /* shut down, no retries */

  /* temperature */
#define TPS546_INIT_OT_WARN_LIMIT  70 /* degrees C */
#define TPS546_INIT_OT_FAULT_LIMIT 80 /* degrees C */
#define TPS546_INIT_OT_FAULT_RESPONSE 0xFF /* wait for cooling, and retry */

  /* timing */
#define TPS546_INIT_TON_DELAY 0
#define TPS546_INIT_TON_RISE 3
#define TPS546_INIT_TON_MAX_FAULT_LIMIT 0
#define TPS546_INIT_TON_MAX_FAULT_RESPONSE 0x3B
#define TPS546_INIT_TOFF_DELAY 0
#define TPS546_INIT_TOFF_FALL 0

/*-------------------------*/

/* PMBUS_ON_OFF_CONFIG initialization values */
#define ON_OFF_CONFIG_PU 0x10   // turn on PU bit
#define ON_OFF_CONFIG_CMD 0x08  // turn on CMD bit
#define ON_OFF_CONFIG_CP 0x00   // turn off CP bit
#define ON_OFF_CONFIG_POLARITY 0x00 // turn off POLARITY bit
#define ON_OFF_CONFIG_DELAY 0x00 // turn off DELAY bit


/* public functions */
int TPS546_init(void);
void TPS546_read_mfr_info(uint8_t *);
void TPS546_set_mfr_info(void);
void TPS546_write_entire_config(void);
int TPS546_get_frequency(void);
void TPS546_set_frequency(int);
int TPS546_get_temperature(void);
void TPS546_set_vout(int millivolts);
void TPS546_show_voltage_settings(void);

#endif /* TPS546_H_ */
