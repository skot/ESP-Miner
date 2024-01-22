#ifndef TPS546_H_
#define TPS546_H_

#define TPS546_I2CADDR          0x24 ///< TPS546 i2c address
#define TPS546_MANUFACTURER_ID 0xFE  ///< Manufacturer ID
#define TPS546_REVISION 0xFF         ///< Chip revision


void TPS546_init(void);
int TPS546_get_temperature(void);
void TPS546_set_vout(int millivolts);

#endif /* TPS546_H_ */
