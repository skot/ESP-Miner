#include "driver/i2c.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "pmbus_commands.h"
#include "TPS546.h"

#define I2C_MASTER_NUM 0 /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */

#define WRITE_BIT      I2C_MASTER_WRITE
#define READ_BIT       I2C_MASTER_READ
#define ACK_CHECK      true
#define NO_ACK_CHECK   false
#define ACK_VALUE      0x0
#define NACK_VALUE     0x1
#define MAX_BLOCK_LEN  32

#define SMBUS_DEFAULT_TIMEOUT (1000 / portTICK_PERIOD_MS)

static const char *TAG = "TPS546.c";


/**
 * @brief SMBus read byte
 */
esp_err_t smb_read_byte(uint8_t command, uint8_t *data)
{
    esp_err_t err = ESP_FAIL;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, TPS546_I2CADDR << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, TPS546_I2CADDR << 1 | READ_BIT, ACK_CHECK);
    i2c_master_read_byte(cmd, data, NACK_VALUE);
    i2c_master_stop(cmd);
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT));
    i2c_cmd_link_delete(cmd);

    // TODO get an actual error status
    return err;
}

/**
 * @brief SMBus read word
 */
esp_err_t smb_read_word(uint8_t command, uint16_t *result)
{
    uint8_t data[2];
    esp_err_t err = ESP_FAIL;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, TPS546_I2CADDR << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, TPS546_I2CADDR << 1 | READ_BIT, ACK_CHECK);
    i2c_master_read(cmd, &data[0], 1, ACK_VALUE);
    i2c_master_read_byte(cmd, &data[1], NACK_VALUE);
    i2c_master_stop(cmd);
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT));
    i2c_cmd_link_delete(cmd);

    *result = (data[1] << 8) + data[0];
    // TODO get an actual error status
    return err;
}

/**
 * @brief SMBus read block
 */
static esp_err_t smb_read_block(uint8_t command, uint8_t * data, uint8_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, TPS546_I2CADDR << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, TPS546_I2CADDR << 1 | READ_BIT, ACK_CHECK);
    uint8_t slave_len = 0;
    i2c_master_read_byte(cmd, &slave_len, ACK_VALUE);
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT));
    i2c_cmd_link_delete(cmd);

    cmd = i2c_cmd_link_create();
    for (size_t i = 0; i < slave_len - 1; ++i)
    {
        i2c_master_read_byte(cmd, &data[i], ACK_VALUE);
    }
    i2c_master_read_byte(cmd, &data[slave_len - 1], NACK_VALUE);
    i2c_master_stop(cmd);
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT));
    i2c_cmd_link_delete(cmd);

    // TODO get an actual error status
    return 0;
}

// Set up the TPS546 regulator and turn it on
void TPS546_init(void)
{
	uint8_t data[6];
    uint8_t u8_value;
    uint16_t u16_value;
    float temp;
    int mantissa, exponent;
    int millivolts;
    float iout;

    ESP_LOGI(TAG, "Initializing the core voltage regulator");

    smb_read_block(PMBUS_IC_DEVICE_ID, data, 6);
    ESP_LOGI(TAG, "Device ID: %02x %02x %02x %02x %02x %02x", data[0], data[1],
                 data[2], data[3], data[4], data[5]);

    smb_read_byte(PMBUS_REVISION, &u8_value);
    ESP_LOGI(TAG, "PMBus revision: %02x", u8_value);

    /* Get temperature (SLINEAR11) */
    ESP_LOGI(TAG, "--------------------------------");
    smb_read_word(PMBUS_READ_TEMPERATURE_1, &u16_value);
    temp = slinear11_2_float(u16_value);
    ESP_LOGI(TAG, "Temp: %2.1f", temp);

    /* Get voltage setting (ULINEAR16) */
    ESP_LOGI(TAG, "--------------------------------");
    smb_read_word(PMBUS_VOUT_COMMAND, &u16_value);
    millivolts = ulinear16_2_int(u16_value);
    ESP_LOGI(TAG, "Vout set to: %d mV", millivolts);

    /* Get voltage output (ULINEAR16) */
//    ESP_LOGI(TAG, "--------------------------------");
//    smb_read_word(PMBUS_READ_VOUT, &u16_value);
//    millivolts = ulinear16_2_int(u16_value);
//    ESP_LOGI(TAG, "Vout measured: %d mV", millivolts);

    /* Get output current (SLINEAR11) */
    ESP_LOGI(TAG, "--------------------------------");
    smb_read_word(PMBUS_READ_IOUT, &u16_value);
    iout = slinear11_2_float(u16_value);
    ESP_LOGI(TAG, "Iout measured: %2.2f", iout);

}


