#include "esp_log.h"
#include <stdio.h>
#include "i2c_bitaxe.h"
#include "EMC2302.h"

#define I2C_MASTER_SCL_IO 48 /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO 47 /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM   0 /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ 400000   /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS 1000

static const char *TAG = "EMC2302";

// run this first. sets up the config register
static i2c_master_dev_handle_t emc2302_dev_handle;

esp_err_t EMC2302_init(bool invertPolarity) {
    if (i2c_bitaxe_add_device(EMC2302_I2CADDR_DEFAULT, &emc2302_dev_handle, TAG) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device");
        return ESP_FAIL;
    }

    if (invertPolarity) {
        ESP_ERROR_CHECK(i2c_bitaxe_register_write_byte(emc2302_dev_handle, EMC2302_PWM_POLARITY, 0b00011111));
    }

    // Set fan range to 00: 500 RPM minimum, TACH count multiplier = 1
    // fan config default before register write: 2B = 00101011
    ESP_ERROR_CHECK(i2c_bitaxe_register_write_byte(emc2302_dev_handle, EMC2302_FAN1_CONFIG1, 0b00001011));

    return ESP_OK;
}

// Sets the fan speed to a given percent
void EMC2302_set_fan_speed(uint8_t devicenum, float percent)
{
    uint8_t speed;
	uint8_t FAN_SETTING_REG = EMC2302_FAN1_SETTING + (devicenum * 0x10);

    speed = (uint8_t) (255.0 * (1.0f-percent));
    ESP_ERROR_CHECK(i2c_bitaxe_register_write_byte(emc2302_dev_handle,FAN_SETTING_REG, speed));
}

// Gets the fan speed
uint16_t EMC2302_get_fan_speed(uint8_t devicenum)
{
    uint8_t tach_lsb, tach_msb;
    uint16_t RPM;
    uint8_t TACH_LSB_REG = EMC2302_TACH1_LSB + (devicenum * 0x10);
    uint8_t TACH_MSB_REG = EMC2302_TACH1_MSB + (devicenum * 0x10);

    ESP_ERROR_CHECK(i2c_bitaxe_register_read(emc2302_dev_handle,TACH_LSB_REG, &tach_lsb, 1));
    ESP_ERROR_CHECK(i2c_bitaxe_register_read(emc2302_dev_handle,TACH_MSB_REG, &tach_msb, 1));

    //ESP_LOGI(TAG, "Raw Fan Speed[%d] = %02X %02X", devicenum, tach_msb, tach_lsb);  // DEBUG
    RPM = (tach_msb << 5) + ((tach_lsb >> 3) & 0x1F);
    RPM = EMC2302_FAN_RPM_NUMERATOR / RPM;
    //ESP_LOGI(TAG, "Fan Speed[%d] = %d RPM", devicenum, RPM);                        // DEBUG

    // DEBUG: get fan config
    //uint8_t fan_conf;
    //uint8_t FAN1_CONFIG1 = EMC2302_FAN1_CONFIG1 + (devicenum * 0x10);
    //ESP_ERROR_CHECK(i2c_bitaxe_register_read(emc2302_dev_handle, FAN1_CONFIG1, &fan_conf, 1));
    //ESP_LOGI(TAG, "Fan config[%d] = %02X", devicenum, fan_conf);
    // DEBUG

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

