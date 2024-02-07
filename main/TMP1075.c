#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c.h"

#include "TMP1075.h"

#define I2C_MASTER_SCL_IO 48        /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO 47        /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM 0            /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ 400000   /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS 1000

static const char *TAG = "TMP1075.c";

bool TMP1075_installed(int);
uint8_t TMP1075_read_temperature(int);

/**
 * @brief Read a sequence of I2C bytes
 */
static esp_err_t register_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, dev_addr, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

/**
 * @brief Write a byte to a I2C register
 */
static esp_err_t register_write_word(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data) {
    int ret;
    uint8_t write_buf[3] = {reg_addr, data[0], data[1]};

    ret = i2c_master_write_to_device(I2C_MASTER_NUM, dev_addr, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    return ret;
}

bool TMP1075_installed(int device_index)
{
    uint8_t data[2];
    esp_err_t result = ESP_OK;

    // read the configuration register
    //ESP_LOGI(TAG, "Reading configuration register");
    ESP_ERROR_CHECK(register_read(TMP1075_I2CADDR_DEFAULT + device_index, TMP1075_CONFIG_REG, data, 2));
    //ESP_LOGI(TAG, "Configuration[%d] = %02X %02X", device_index, data[0], data[1]);

    return (result == ESP_OK?true:false);
}

uint8_t TMP1075_read_temperature(int device_index)
{
    uint8_t data[2];

    ESP_ERROR_CHECK(register_read(TMP1075_I2CADDR_DEFAULT + device_index, TMP1075_TEMP_REG, data, 2));
    //ESP_LOGI(TAG, "Raw Temperature = %02X %02X", data[0], data[1]);
    //ESP_LOGI(TAG, "Temperature[%d] = %d", device_index, data[0]);
    return data[0];
}

