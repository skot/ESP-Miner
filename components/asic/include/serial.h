#ifndef SERIAL_H_
#define SERIAL_H_

#define SERIAL_BUF_SIZE 16
#define CHUNK_SIZE 1024

static uint8_t asic_timeout_counter = 0;
static uint8_t asic_timeout_warning_threshold = 2;
static int uart_timeout_ms = 10000;

int SERIAL_send(uint8_t *, int, bool);
void SERIAL_init(void);
void SERIAL_debug_rx(void);
int16_t SERIAL_rx(uint8_t *, uint16_t, uint16_t);
void SERIAL_clear_buffer(void);
void SERIAL_set_baud(int baud);

#endif /* SERIAL_H_ */