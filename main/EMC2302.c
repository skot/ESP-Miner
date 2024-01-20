#include "driver/i2c.h"
#include "esp_log.h"
#include <stdio.h>

#include "EMC2302.h"

#define I2C_MASTER_SCL_IO 48 /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO 47 /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM                                                                                                             \
    0 /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ 400000   /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS 1000

static const char *TAG = "EMC2302.c";

/**
 * @brief Read a sequence of I2C bytes
 */
static esp_err_t register_read(uint8_t reg_addr, uint8_t * data, size_t len)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, EMC2302_I2CADDR_DEFAULT, &reg_addr, 1, data, len,
                                        I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

/**
 * @brief Write a byte to a I2C register
 */
static esp_err_t register_write_byte(uint8_t reg_addr, uint8_t data)
{
    int ret;
    uint8_t write_buf[2] = {reg_addr, data};

    ret = i2c_master_write_to_device(I2C_MASTER_NUM, EMC2302_I2CADDR_DEFAULT, write_buf, sizeof(write_buf),
                                     I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    return ret;
}

// run this first. sets up the PWM polarity register
void EMC2302_init(bool invertPolarity)
{

    if (invertPolarity) {
        ESP_ERROR_CHECK(register_write_byte(EMC2302_PWM_POLARITY, 0b00011111));
    }
}

// Sets the fan speed to a given percent
void EMC2302_set_fan_speed(uint8_t devicenum, float percent)
{
    uint8_t speed;
	uint8_t FAN_SETTING_REG = EMC2302_FAN1_SETTING + (devicenum * 0x10);

    speed = (uint8_t) (63.0 * percent);
    ESP_ERROR_CHECK(register_write_byte(FAN_SETTING_REG, speed));
}

// Gets the fan speed
uint16_t EMC2302_get_fan_speed(uint8_t devicenum)
{
    uint8_t tach_lsb, tach_msb;
    uint16_t RPM;
    uint8_t TACH_LSB_REG = EMC2302_TACH1_LSB + (devicenum * 0x10);
    uint8_t TACH_MSB_REG = EMC2302_TACH1_MSB + (devicenum * 0x10);

    ESP_ERROR_CHECK(register_read(TACH_LSB_REG, &tach_lsb, 1));
    ESP_ERROR_CHECK(register_read(TACH_MSB_REG, &tach_msb, 1));

    ESP_LOGI(TAG, "Raw Fan Speed[%d] = %02X %02X", devicenum, tach_msb, tach_lsb);
    RPM = (tach_msb << 5) + ((tach_lsb >> 3) & 0x1F);
    ESP_LOGI(TAG, "Fan Speed[%d] = %d RPM", devicenum, RPM);

    return RPM;
}

float EMC2302_get_external_temp(void)
{
    // We don't have temperature on this chip, so fake it
    return 0;
}

uint8_t EMC2302_get_internal_temp(void)
{
    // We don't have temperature on this chip, so fake it
    return 0;
}

