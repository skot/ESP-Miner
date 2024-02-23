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
    uint8_t read_byte;
    uint8_t fan_config1;

    // Read the manufacturer ID
    ESP_ERROR_CHECK(register_read(EMC2302_MANUFACTURER_ID, &read_byte, 1));
    ESP_LOGI(TAG, "Manufacturer ID = %02x", read_byte);    

    // Read the product ID
    ESP_ERROR_CHECK(register_read(EMC2302_PRODUCT_ID, &read_byte, 1));
    ESP_LOGI(TAG, "Product ID = %02x", read_byte);    

    // Read the 
    ESP_ERROR_CHECK(register_read(EMC2302_SILICON_REVISION, &read_byte, 1));
    ESP_LOGI(TAG, "Silicon Revision = %02x", read_byte);    

    fan_config1 = 0x98;  // 10011000
    ESP_ERROR_CHECK(register_write_byte(EMC2302_FAN1_CONFIG1, fan_config1));
    ESP_ERROR_CHECK(register_write_byte(EMC2302_FAN2_CONFIG1, fan_config1));

    //if (invertPolarity) {
    //    ESP_ERROR_CHECK(register_write_byte(EMC2302_PWM_POLARITY, 0b00011111));
    //}
}

// Sets the fan speed to a given percent
void EMC2302_set_fan_speed(uint8_t devicenum, float percent)
{
    int max_rpm = 2400;
    int req_rpm = max_rpm * (percent / 100);
    uint16_t tach_counts;
    uint8_t TACH_MSB_REG = EMC2302_TACH1_TARGET_MSB + (devicenum * 0x10);
    uint8_t TACH_LSB_REG = EMC2302_TACH1_TARGET_LSB + (devicenum * 0x10);
    uint8_t tach_counts_MSB;
    uint8_t tach_counts_LSB;

    int poles = 4;    // motor poles
    int edges = 9;    // motor edges
    int ftach = 32768;  // tach clock frequency

    //ESP_LOGI(TAG, "SET-Fan RPM requested (%d%%) %d", (int)percent, req_rpm);

    tach_counts = (uint16_t) ((edges - 1)/poles * (1.0f/req_rpm) * ftach * 60);
    tach_counts_MSB = tach_counts >> 5;
    tach_counts_LSB = (tach_counts & 0x001F) << 3;

    ESP_ERROR_CHECK( register_write_byte(TACH_MSB_REG, tach_counts_MSB ));
    ESP_ERROR_CHECK( register_write_byte(TACH_LSB_REG, tach_counts_LSB ));
}

// Gets the fan speed
uint16_t EMC2302_get_fan_speed(uint8_t devicenum)
{
    uint8_t tach_lsb, tach_msb;
    uint16_t tach_counts = 1;
    uint16_t rpm;
    uint8_t TACH_LSB_REG = EMC2302_TACH1_LSB + (devicenum * 0x10);
    uint8_t TACH_MSB_REG = EMC2302_TACH1_MSB + (devicenum * 0x10);

    int poles = 4;    // motor poles
    int edges = 9;    // motor edges
    int ftach = 32768;  // tach clock frequency

    ESP_ERROR_CHECK(register_read(TACH_MSB_REG, &tach_msb, 1));
    ESP_ERROR_CHECK(register_read(TACH_LSB_REG, &tach_lsb, 1));
    //ESP_LOGI(TAG, "GET-Tach counts reg: %02x %02x", tach_msb, tach_lsb);

    if ((tach_msb == 0xff) && (tach_msb == 0xF8)) {
        rpm = 0;
    } else {
        tach_counts = (tach_msb << 5) + ((tach_lsb >> 3) & 0x1F);
        rpm = (uint16_t) ((edges - 1)/poles * (1.0f/tach_counts) * ftach * 60);
    }
    ESP_LOGI(TAG, "GET-Fan Speed[%d] = %d RPM", devicenum, rpm);

    return rpm;
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

