#include <stdio.h>
#include "esp_log.h"

#include "i2c_bitaxe.h"
#include "EMC2103.h"

static const char * TAG = "EMC2103";

static i2c_master_dev_handle_t EMC2103_dev_handle;

/**
 * @brief Initialize the EMC2103 sensor.
 *
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t EMC2103_init(bool invertPolarity) {

    if (i2c_bitaxe_add_device(EMC2103_I2CADDR_DEFAULT, &EMC2103_dev_handle, TAG) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device");
        return ESP_FAIL;
    }

    // set the TACH input
    ESP_ERROR_CHECK(i2c_bitaxe_register_write_byte(EMC2103_dev_handle, EMC2103_REG_CONFIG, 0x04));

    if (invertPolarity) {
        ESP_ERROR_CHECK(i2c_bitaxe_register_write_byte(EMC2103_dev_handle, EMC2103_FAN_CONFIG, 0b00100011));
    }



    // We're using default filtering and conversion, no need to set them again.
    // //set filtering
    // ESP_ERROR_CHECK(i2c_bitaxe_register_write_byte(EMC2103_dev_handle, EMC2103_TEMP_FILTER, EMC2103_DEFAULT_FILTER));

    // //set conversion rate
    // ESP_ERROR_CHECK(i2c_bitaxe_register_write_byte(EMC2103_dev_handle, EMC2103_REG_DATA_RATE, EMC2103_DEFAULT_DATARATE));

    return ESP_OK;

}

void EMC2103_set_ideality_factor(uint8_t ideality){
    //set Ideality Factor
    ESP_ERROR_CHECK(i2c_bitaxe_register_write_byte(EMC2103_dev_handle, EMC2103_IDEALITY_FACTOR, ideality));
}

void EMC2103_set_beta_compensation(uint8_t beta){
    //set Beta Compensation
    ESP_ERROR_CHECK(i2c_bitaxe_register_write_byte(EMC2103_dev_handle, EMC2103_BETA_COMPENSATION, beta));

}

// takes a fan speed percent
void EMC2103_set_fan_speed(float percent)
{
    uint8_t speed;

    speed = (uint8_t) (63.0 * percent);
    ESP_ERROR_CHECK(i2c_bitaxe_register_write_byte(EMC2103_dev_handle, EMC2103_REG_FAN_SETTING, speed));
}

// RPM = 5400000/reading
uint16_t EMC2103_get_fan_speed(void)
{
    uint8_t tach_lsb, tach_msb;
    uint16_t reading;
    uint16_t RPM;

    ESP_ERROR_CHECK(i2c_bitaxe_register_read(EMC2103_dev_handle, EMC2103_TACH_LSB, &tach_lsb, 1));
    ESP_ERROR_CHECK(i2c_bitaxe_register_read(EMC2103_dev_handle, EMC2103_TACH_MSB, &tach_msb, 1));

    // ESP_LOGI(TAG, "Raw Fan Speed = %02X %02X", tach_msb, tach_lsb);

    reading = tach_lsb | (tach_msb << 8);
    RPM = 5400000 / reading;

    // ESP_LOGI(TAG, "Fan Speed = %d RPM", RPM);
    if (RPM == 82) {
        return 0;
    }
    return RPM;
}

float EMC2103_get_external_temp(void)
{
    uint8_t temp_msb, temp_lsb;
    uint16_t reading;

    ESP_ERROR_CHECK(i2c_bitaxe_register_read(EMC2103_dev_handle, EMC2103_EXTERNAL_TEMP_MSB, &temp_msb, 1));
    ESP_ERROR_CHECK(i2c_bitaxe_register_read(EMC2103_dev_handle, EMC2103_EXTERNAL_TEMP_LSB, &temp_lsb, 1));
    
    // Combine MSB and LSB, and then right shift to get 11 bits
    reading = (temp_msb << 8) | temp_lsb;
    reading >>= 5;  // Now, `reading` contains an 11-bit signed value

    // Cast `reading` to a signed 16-bit integer
    int16_t signed_reading = (int16_t)reading;

    // If the 11th bit (sign bit in 11-bit data) is set, extend the sign
    if (signed_reading & 0x0400) {
        signed_reading |= 0xF800;  // Set upper bits to extend the sign
    }

    if (signed_reading == EMC2103_TEMP_FAULT_OPEN_CIRCUIT) {
        ESP_LOGE(TAG, "EMC2103 TEMP_FAULT_OPEN_CIRCUIT: %04X", signed_reading);
    }
    if (signed_reading == EMC2103_TEMP_FAULT_SHORT) {
        ESP_LOGE(TAG, "EMC2103 TEMP_FAULT_SHORT: %04X", signed_reading);
    }

    // Convert the signed reading to temperature in Celsius
    float result = (float)signed_reading / 8.0;

    return result;
}

uint8_t EMC2103_get_internal_temp(void)
{
    uint8_t temp;
    ESP_ERROR_CHECK(i2c_bitaxe_register_read(EMC2103_dev_handle, EMC2103_INTERNAL_TEMP, &temp, 1));
    return temp;
}
