#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "i2c_master.h"

#define I2C_MASTER_SCL_IO 48        /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO 47        /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_FREQ_HZ 400000   /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */

//setup I2C semaphore to protect I2C access
SemaphoreHandle_t i2c_sem = NULL;

/**
 * @brief i2c master initialization
 */
esp_err_t i2c_master_init(void)
{
    int i2c_master_port = I2C_MASTER_NUM;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_param_config(i2c_master_port, &conf);

    //init I2C semaphore
    i2c_sem = xSemaphoreCreateMutex();

    return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

/**
 * @brief i2c master delete
 */
esp_err_t i2c_master_delete(void)
{
    //delete I2C semaphore
    vSemaphoreDelete(i2c_sem);

    return i2c_driver_delete(I2C_MASTER_NUM);
}

/**
 * @brief Read a sequence of I2C bytes
 */
esp_err_t i2c_master_register_read(uint8_t device_address, uint8_t reg_addr, uint8_t * data, size_t len)
{
    esp_err_t return_value;
    //wait for I2C access
    if (xSemaphoreTake(i2c_sem, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS) != pdTRUE) {
        return ESP_FAIL;
    }
    return_value = i2c_master_write_read_device(I2C_MASTER_NUM, device_address, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    //release I2C access
    xSemaphoreGive(i2c_sem);

    return return_value;
}

/**
 * @brief Write a byte to a I2C register
 */
esp_err_t i2c_master_register_write_byte(uint8_t device_address, uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};

    esp_err_t return_value;
    //wait for I2C access
    if (xSemaphoreTake(i2c_sem, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS) != pdTRUE) {
        return ESP_FAIL;
    }

    return_value = i2c_master_write_to_device(I2C_MASTER_NUM, device_address, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    //release I2C access
    xSemaphoreGive(i2c_sem);

    return return_value;
}

/**
 * @brief Write multiple bytes to a I2C register
 */
esp_err_t i2c_master_register_write_bytes(uint8_t device_address, uint8_t * data, uint16_t len)
{

    esp_err_t return_value;

    //wait for I2C access
    if (xSemaphoreTake(i2c_sem, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS) != pdTRUE) {
        ESP_LOGE("I2C", "Failed to take I2C semaphore");
        return ESP_FAIL;
    }

    return_value = i2c_master_write_to_device(I2C_MASTER_NUM, device_address, data, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    //release I2C access
    xSemaphoreGive(i2c_sem);

    return return_value;
}

/**
 * @brief Write a word to a I2C register
 */
esp_err_t i2c_master_register_write_word(uint8_t device_address, uint8_t reg_addr, uint16_t data)
{
    uint8_t write_buf[3] = {reg_addr, (data >> 8) & 0xFF, data & 0xFF};
    esp_err_t return_value;
    //wait for I2C access
    if (xSemaphoreTake(i2c_sem, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS) != pdTRUE) {
        return ESP_FAIL;
    }

    return_value = i2c_master_write_to_device(I2C_MASTER_NUM, device_address, write_buf, sizeof(write_buf),
                                     I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    //release I2C access
    xSemaphoreGive(i2c_sem);

    return return_value;
}
