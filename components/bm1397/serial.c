#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/uart.h"

#include "esp_log.h"
#include "soc/uart_struct.h"

#include "bm1397.h"
#include "serial.h"
#include "utils.h"

#define ECHO_TEST_TXD (17)
#define ECHO_TEST_RXD (18)
#define BUF_SIZE (1024)

static const char *TAG = "serial";

void SERIAL_init(void)
{
    ESP_LOGI(TAG, "Initializing serial");
    // Configure UART1 parameters
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };
    // Configure UART1 parameters
    uart_param_config(UART_NUM_1, &uart_config);
    // Set UART1 pins(TX: IO17, RX: I018)
    uart_set_pin(UART_NUM_1, ECHO_TEST_TXD, ECHO_TEST_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // Install UART driver (we don't need an event queue here)
    // tx buffer 0 so the tx time doesn't overlap with the job wait time
    //  by returning before the job is written
    uart_driver_install(UART_NUM_1, BUF_SIZE * 2, BUF_SIZE * 2, 0, NULL, 0);
}

void SERIAL_set_baud(int baud)
{
    ESP_LOGI(TAG, "Changing UART baud to %i", baud);
    uart_set_baudrate(UART_NUM_1, baud);
}

int SERIAL_send(uint8_t *data, int len, bool debug)
{
    if (debug)
    {
        printf("->");
        prettyHex((unsigned char *)data, len);
        printf("\n");
    }

    return uart_write_bytes(UART_NUM_1, (const char *)data, len);
}

/// @brief reads serial bytes into a buffer
/// @param buf 
/// @param size 
/// @param timeout_ms 
/// @return 
int16_t SERIAL_rx(uint8_t *buf, uint16_t size, uint16_t timeout_ms)
{
    int16_t bytes_read = uart_read_bytes(UART_NUM_1, buf, size, timeout_ms / portTICK_PERIOD_MS);
    // if (bytes_read > 0) {
    //     printf("rx: ");
    //     prettyHex((unsigned char*) buf, bytes_read);
    //     printf("\n");
    // }
    return bytes_read;
}

void SERIAL_debug_rx(void)
{
    int ret;
    uint8_t buf[CHUNK_SIZE];

    ret = SERIAL_rx(buf, 100, 20);
    if (ret < 0)
    {
        fprintf(stderr, "unable to read data\n");
        return;
    }

    memset(buf, 0, 1024);
}

void SERIAL_clear_buffer(void)
{
    uart_flush(UART_NUM_1);
}

/**
  * @brief recieve packet with 0xaa 0x55 header
  * @param data - buffer for received serial data
  * @param length - length of expected packet
  */
 void *SERIAL_rx_aa55(uint8_t *data,const int length) {
     for(int len=0; len < length;) {
         // wait for a response, wait time is pretty arbitrary
         int received = SERIAL_rx(data+len, length-len, 60000);
         if (received < 0) {
             ESP_LOGI(TAG, "Error in serial RX");
             return NULL;
         } else if (received == 0) {
             // Didn't find a solution, restart and try again
             return NULL;
         }

         if (len+received > 2) {
             // valid start
             if (data[0] == 0xAA && data[1] == 0x55) {
                 len += received;
             } else {
                 for(int count = 1; count < len + received; ++count) {
                     if(*(data+count) == 0xAA) {
                         // move to head and adjust read length
                         memmove(data, data+count, len+received-count);
                         len+=received-count;
                         break;
                     }
                 }
             }
         } else {
             len+=received;
         }
     }
     return data;
 }
