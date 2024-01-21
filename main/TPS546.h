#ifndef TPS546_H_
#define TPS546_H_

#define TPS546_I2CADDR          0x24 ///< TPS546 i2c address
#define TPS546_MANUFACTURER_ID 0xFE  ///< Manufacturer ID
#define TPS546_REVISION 0xFF         ///< Chip revision


void TPS546_init(void);
//void TPS546_set_voltage(float);
//void TPS546_get_status(void);

#endif /* TPS546_H_ */
