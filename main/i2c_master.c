#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "i2c_master.h"
#include "pmbus_commands.h"

#define I2C_MASTER_SCL_IO 48        /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO 47        /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_FREQ_HZ 400000   /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */

//SMbus defines
#define WRITE_BIT      I2C_MASTER_WRITE
#define READ_BIT       I2C_MASTER_READ
#define ACK_CHECK      true
#define NO_ACK_CHECK   false
#define ACK_VALUE      0x0
#define NACK_VALUE     0x1
#define MAX_BLOCK_LEN  32

#define SMBUS_DEFAULT_TIMEOUT (1000 / portTICK_PERIOD_MS)


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



////////SMBUS stuff is here too

/**
 * @brief SMBus read byte
 */
esp_err_t smb_read_byte(uint8_t device_address, uint8_t command, uint8_t *data)
{
    esp_err_t err = ESP_FAIL;

    //wait for I2C access
    if (xSemaphoreTake(i2c_sem, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS) != pdTRUE) {
        ESP_LOGE("I2C", "Failed to take I2C semaphore");
        return ESP_FAIL;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, device_address << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, device_address << 1 | READ_BIT, ACK_CHECK);
    i2c_master_read_byte(cmd, data, NACK_VALUE);
    i2c_master_stop(cmd);
    i2c_set_timeout(I2C_MASTER_NUM, 20);
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT));
    i2c_cmd_link_delete(cmd);

    //release I2C access
    xSemaphoreGive(i2c_sem);

    // return get an actual error status
    return err;
}

/**
 * @brief SMBus write byte
 */
esp_err_t smb_write_byte(uint8_t device_address, uint8_t command, uint8_t data)
{
    esp_err_t err = ESP_FAIL;

    //wait for I2C access
    if (xSemaphoreTake(i2c_sem, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS) != pdTRUE) {
        ESP_LOGE("I2C", "Failed to take I2C semaphore");
        return ESP_FAIL;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, device_address << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_write_byte(cmd, data, ACK_CHECK);
    i2c_master_stop(cmd);
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT));
    i2c_cmd_link_delete(cmd);

    //release I2C access
    xSemaphoreGive(i2c_sem);

    // TODO return an actual error status
    return err;
}

/**
 * @brief SMBus read word
 */
esp_err_t smb_read_word(uint8_t device_address, uint8_t command, uint16_t *result)
{
    uint8_t data[2];
    esp_err_t err = ESP_FAIL;

    //wait for I2C access
    if (xSemaphoreTake(i2c_sem, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS) != pdTRUE) {
        ESP_LOGE("I2C", "Failed to take I2C semaphore");
        return ESP_FAIL;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, device_address << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, device_address << 1 | READ_BIT, ACK_CHECK);
    i2c_master_read(cmd, &data[0], 1, ACK_VALUE);
    i2c_master_read_byte(cmd, &data[1], NACK_VALUE);
    i2c_master_stop(cmd);
    i2c_set_timeout(I2C_MASTER_NUM, 20);
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT));
    i2c_cmd_link_delete(cmd);

    *result = (data[1] << 8) + data[0];

    //release I2C access
    xSemaphoreGive(i2c_sem);

    // TODO return an actual error status
    return err;
}

/**
 * @brief SMBus write word
 */
esp_err_t smb_write_word(uint8_t device_address, uint8_t command, uint16_t data)
{
    esp_err_t err = ESP_FAIL;

    //wait for I2C access
    if (xSemaphoreTake(i2c_sem, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS) != pdTRUE) {
        ESP_LOGE("I2C", "Failed to take I2C semaphore");
        return ESP_FAIL;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, device_address << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_write_byte(cmd, (uint8_t)(data & 0x00FF), ACK_CHECK);
    i2c_master_write_byte(cmd, (uint8_t)((data & 0xFF00) >> 8), NACK_VALUE);
    i2c_master_stop(cmd);
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT));
    i2c_cmd_link_delete(cmd);

    //release I2C access
    xSemaphoreGive(i2c_sem);

    // TODO return an actual error status
    return err;
}

/**
 * @brief SMBus read block
 */
esp_err_t smb_read_block(uint8_t device_address, uint8_t command, uint8_t *data, uint8_t len)
{

    //wait for I2C access
    if (xSemaphoreTake(i2c_sem, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS) != pdTRUE) {
        ESP_LOGE("I2C", "Failed to take I2C semaphore");
        return ESP_FAIL;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, device_address << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, device_address << 1 | READ_BIT, ACK_CHECK);
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

    //release I2C access
    xSemaphoreGive(i2c_sem);

    // TODO return an actual error status
    return 0;
}

/**
 * @brief SMBus write block
 */
esp_err_t smb_write_block(uint8_t device_address, uint8_t command, uint8_t *data, uint8_t len)
{
    //wait for I2C access
    if (xSemaphoreTake(i2c_sem, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS) != pdTRUE) {
        ESP_LOGE("I2C", "Failed to take I2C semaphore");
        return ESP_FAIL;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, device_address << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_write_byte(cmd, len, ACK_CHECK);
    for (size_t i = 0; i < len; ++i)
    {
        i2c_master_write_byte(cmd, data[i], ACK_CHECK);
    }
    i2c_master_stop(cmd);
    i2c_set_timeout(I2C_MASTER_NUM, 20);
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT));
    i2c_cmd_link_delete(cmd);

    //release I2C access
    xSemaphoreGive(i2c_sem);
    
    // TODO return an actual error status
    return 0;
}
