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
static esp_err_t smb_read_byte(uint8_t command, uint8_t *data)
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
 * @brief SMBus write byte
 */
static esp_err_t smb_write_byte(uint8_t command, uint8_t data)
{
    esp_err_t err = ESP_FAIL;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, TPS546_I2CADDR << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_write_byte(cmd, data, ACK_CHECK);
    i2c_master_stop(cmd);
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT));
    i2c_cmd_link_delete(cmd);

    // TODO get an actual error status
    return err;
}

/**
 * @brief SMBus read word
 */
static esp_err_t smb_read_word(uint8_t command, uint16_t *result)
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
 * @brief SMBus write word
 */
static esp_err_t smb_write_word(uint8_t command, uint16_t data)
{
    esp_err_t err = ESP_FAIL;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, TPS546_I2CADDR << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_write_byte(cmd, (uint8_t)(data & 0x00FF), ACK_CHECK);
    i2c_master_write_byte(cmd, (uint8_t)((data & 0xFF00) >> 8), NACK_VALUE);
    i2c_master_stop(cmd);
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT));
    i2c_cmd_link_delete(cmd);

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


/**
 * @brief Convert an SLINEAR11 value into an int
 */
static int slinear11_2_int(uint16_t value)
{
    int exponent, mantissa;
    float result;

    // First 5 bits is exponent in twos-complement
    // check the first bit of the exponent to see if its negative
    if (value & 0x8000) {
        // exponent is negative
        exponent = -1 * (((~value >> 11) & 0x001F) + 1);
    } else {
        exponent = (value >> 11);
    }
    // last 11 bits is the mantissa in twos-complement
    // check the first bit of the mantissa to see if its negative
    if (value & 0x400) {
        // mantissa is negative
        mantissa = -1 * ((~value & 0x07FF) + 1);
    } else {
        mantissa = (value & 0x07FF);
    }

    // calculate result (mantissa * 2^exponent)
    result = mantissa * powf(2.0, exponent);
    ESP_LOGI(TAG, "result: %f", result);
    return (int)result;
}

/**
 * @brief Convert an int value into an SLINEAR11
 */
static uint16_t int_2_slinear11(int value)
{
    float mantissa;
    int exponent;
    uint16_t result = 0;

    mantissa = frexp(value, &exponent);
    ESP_LOGI(TAG, "mantissa: %f, exponent: %d", mantissa, exponent);

    result = (mantissa * 1024);
//  ESP_LOGI(TAG, "result: %04x", result);
    // TPS546.c: Writing new frequency: 550
    // TPS546.c: result: 0.537109, exponent: 10

    // First 5 bits is exponent in twos-complement
    // check the first bit of the exponent to see if its negative
//    if (value & 0x8000) {
        // exponent is negative
//        exponent = -1 * (((~value >> 11) & 0x001F) + 1);
//    } else {
//        exponent = (value >> 11);
//    }
    // last 11 bits is the mantissa in twos-complement
    // check the first bit of the mantissa to see if its negative
//    if (value & 0x400) {
        // mantissa is negative
//        mantissa = -1 * ((~value & 0x07FF) + 1);
//    } else {
//        mantissa = (value & 0x07FF);
//    }

    // TPS546.c: Read Freq: 1234
    // TPS546.c: result: 2256.000000

    // calculate result (mantissa * 2^exponent)
//    result = mantissa * powf(2.0, exponent);
    return result;
}

/**
 * @brief Convert a ULINEAR16 value into an int
 */
static int ulinear16_2_int(uint16_t value)
{
    float exponent;
    int result;
    uint8_t voutmode;

    /* the exponent comes from VOUT_MODE bits[4..0] */
    /* in twos-complement */
    smb_read_byte(PMBUS_VOUT_MODE, &voutmode);
    if (voutmode & 0x10) {
        // exponent is negative
        exponent = -1 * ((~voutmode & 0x1F) + 1);
    } else {
        exponent = (voutmode & 0x1F);
    }

    result = (value * powf(2.0, exponent)) * 1000;
    return result;
}

/**
 * @brief Convert an int value into a ULINEAR16
 */
static uint16_t int_2_ulinear16(int value)
{
    uint8_t voutmode;
    float exponent;
    int result;

    /* the exponent comes from VOUT_MODE bits[4..0] */
    /* in twos-complement */
    smb_read_byte(PMBUS_VOUT_MODE, &voutmode);
    if (voutmode & 0x10) {
        // exponent is negative
        exponent = -1 * ((~voutmode & 0x1F) + 1);
    } else {
        exponent = (voutmode & 0x1F);
    }

    ESP_LOGI(TAG, "Exp: %f", exponent);
    result = (value / powf(2.0, exponent)) / 1000;
    //result = (value * powf(2.0, exponent)) * 1000;

    return result;
}

/*--- Public TPS546 functions ---*/

// Set up the TPS546 regulator and turn it on
void TPS546_init(void)
{
	uint8_t data[6];
    uint8_t u8_value;
    uint16_t u16_value;
    int i_value;
    int millivolts;
    int vmax;
    float f_value;
    int iout;
    ESP_LOGI(TAG, "Initializing the core voltage regulator");

    smb_read_block(PMBUS_IC_DEVICE_ID, data, 6);
    ESP_LOGI(TAG, "Device ID: %02x %02x %02x %02x %02x %02x", data[0], data[1],
                 data[2], data[3], data[4], data[5]);

    smb_read_byte(PMBUS_REVISION, &u8_value);
    ESP_LOGI(TAG, "PMBus revision: %02x", u8_value);

    /* Show temperature */
    ESP_LOGI(TAG, "--------------------------------");
    ESP_LOGI(TAG, "Temp: %d", TPS546_get_temperature());

    /* Show switching frequency */
    TPS546_get_frequency();
    TPS546_set_frequency(650);

    /* Show voltage settings */
    TPS546_show_voltage_settings();

    /* Test changing value */
//    u16_value = int_2_ulinear16(1300);
//    smb_write_word(PMBUS_VOUT_MAX, u16_value);
//    ESP_LOGI(TAG, "Vout Max changed to 1300");

//    smb_read_word(PMBUS_VOUT_MAX, &u16_value);
//    ESP_LOGI(TAG, "VOUT_MAX: %04x", u16_value);
//    vmax = ulinear16_2_int(u16_value);
//    ESP_LOGI(TAG, "Vout Max set to: %d mV", vmax);

    ESP_LOGI(TAG, "-----------CURRENT---------------------");
    /* Get output current (SLINEAR11) */
    smb_read_word(PMBUS_READ_IOUT, &u16_value);
    iout = slinear11_2_int(u16_value);
    ESP_LOGI(TAG, "Iout measured: %d", iout);

    /* Get voltage output (ULINEAR16) */
    // This gets a timeout, don't know why.  clock stretching?
//    ESP_LOGI(TAG, "--------------------------------");
//    smb_read_word(PMBUS_READ_VOUT, &u16_value);
//    millivolts = ulinear16_2_int(u16_value);
//    ESP_LOGI(TAG, "Vout measured: %d mV", millivolts);

}

int TPS546_get_frequency(void)
{
    uint16_t value;
    int freq;

    smb_read_word(PMBUS_FREQUENCY_SWITCH, &value);
    ESP_LOGI(TAG, "Read Freq: %04x", value);
    freq = slinear11_2_int(value);

    ESP_LOGI(TAG, "Frequency: %d", freq);
    return (int)freq;
}

void TPS546_set_frequency(int newfreq)
{
    uint16_t value;
    int freq;

    ESP_LOGI(TAG, "Writing new frequency: %d", newfreq);
    value = int_2_slinear11(newfreq);
    //value = 0xC999;
    ESP_LOGI(TAG, "New value: 0x%04x", value);
    //smb_write_word(PMBUS_FREQUENCY_SWITCH, value);

    ESP_LOGI(TAG, "Checking conversion...");
    freq = slinear11_2_int(value);
    ESP_LOGI(TAG, "Converted value: %d", freq);

    //ESP_LOGI(TAG, "Frequency: %d", (int)freq);
    //return (int)freq;
}

int TPS546_get_temperature(void)
{
    uint16_t value;
    int temp;

    smb_read_word(PMBUS_READ_TEMPERATURE_1, &value);
    temp = slinear11_2_int(value);
    return temp;
}

void TPS546_set_vout(int millivolts)
{
    uint16_t value;

    value = int_2_ulinear16(millivolts);
    smb_write_word(PMBUS_VOUT_COMMAND, value);
    ESP_LOGI(TAG, "Vout changed to %d mV", millivolts);
}

void TPS546_show_voltage_settings(void)
{
    uint16_t u16_value;
    int i_value;
    float f_value;
    int millivolts;

    ESP_LOGI(TAG, "-----------VOLTAGE---------------------");
    /* VIN_ON SLINEAR11 */
    smb_read_word(PMBUS_VIN_ON, &u16_value);
    i_value = slinear11_2_int(u16_value);
    ESP_LOGI(TAG, "VIN ON set to: %d", i_value);

    /* VIN_OFF SLINEAR11 */
    smb_read_word(PMBUS_VIN_OFF, &u16_value);
    i_value = slinear11_2_int(u16_value);
    ESP_LOGI(TAG, "VIN OFF set to: %d", i_value);

    /* VOUT_MAX */
    smb_read_word(PMBUS_VOUT_MAX, &u16_value);
    i_value = ulinear16_2_int(u16_value);
    ESP_LOGI(TAG, "Vout Max set to: %d mV", i_value);

    /* VOUT_OV_FAULT_LIMIT */
    smb_read_word(PMBUS_VOUT_OV_FAULT_LIMIT, &u16_value);
    i_value = ulinear16_2_int(u16_value);
    ESP_LOGI(TAG, "Vout OV Fault Limit: %d mV", i_value);

    /* VOUT_OV_WARN_LIMIT */
    smb_read_word(PMBUS_VOUT_OV_WARN_LIMIT, &u16_value);
    i_value = ulinear16_2_int(u16_value);
    ESP_LOGI(TAG, "Vout OV Warn Limit: %d mV", i_value);

    /* VOUT_MARGIN_HIGH */
    smb_read_word(PMBUS_VOUT_MARGIN_HIGH, &u16_value);
    i_value = ulinear16_2_int(u16_value);
    ESP_LOGI(TAG, "Vout Margin HIGH: %d mV", i_value);

    /* --- VOUT_COMMAND --- */
    smb_read_word(PMBUS_VOUT_COMMAND, &u16_value);
    millivolts = ulinear16_2_int(u16_value);
    ESP_LOGI(TAG, "Vout set to: %d mV", millivolts);
    //u16_value = int_2_ulinear16(millivolts);
    //ESP_LOGI(TAG, "Converted back: %04x", u16_value);

    /* VOUT_MARGIN_LOW */
    smb_read_word(PMBUS_VOUT_MARGIN_LOW, &u16_value);
    i_value = ulinear16_2_int(u16_value);
    ESP_LOGI(TAG, "Vout Margin LOW: %d mV", i_value);

    /* VOUT_UV_WARN_LIMIT */
    smb_read_word(PMBUS_VOUT_UV_WARN_LIMIT, &u16_value);
    i_value = ulinear16_2_int(u16_value);
    ESP_LOGI(TAG, "Vout UV Warn Limit: %d mV", i_value);

    /* VOUT_UV_FAULT_LIMIT */
    smb_read_word(PMBUS_VOUT_UV_FAULT_LIMIT, &u16_value);
    i_value = ulinear16_2_int(u16_value);
    ESP_LOGI(TAG, "Vout UV Fault Limit: %d mV", i_value);

    /* VOUT_MIN */
    smb_read_word(PMBUS_VOUT_MIN, &u16_value);
    i_value = ulinear16_2_int(u16_value);
    ESP_LOGI(TAG, "Vout Min set to: %d mV", i_value);
}

