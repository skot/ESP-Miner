#ifndef CRC_H_
#define CRC_H_

#ifdef __cplusplus
extern "C" {
#endif

uint8_t crc5(uint8_t *data, uint8_t len);
unsigned short crc16(const unsigned char *buffer, int len);
unsigned short crc16_false(const unsigned char *buffer, int len);

#ifdef __cplusplus
}
#endif

#endif // PRETTY_H_