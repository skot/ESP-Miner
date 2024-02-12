#include "DS4432U.h"
#include "EMC2101.h"
#include "INA260.h"
#include "TPS546.h"
#include "TMP1075.h"
#include "bm1397.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "global_state.h"
#include "math.h"
#include "mining.h"
#include "nvs_config.h"
#include "serial.h"
#include <string.h>

#define POLL_RATE 2000
#define MAX_TEMP 90.0
#define THROTTLE_TEMP 75.0
#define THROTTLE_TEMP_RANGE (MAX_TEMP - THROTTLE_TEMP)

#define TPS546_THROTTLE_TEMP 105.0

#define VOLTAGE_START_THROTTLE 4900
#define VOLTAGE_MIN_THROTTLE 3500
#define VOLTAGE_RANGE (VOLTAGE_START_THROTTLE - VOLTAGE_MIN_THROTTLE)

static const char * TAG = "power_management";

static float _fbound(float value, float lower_bound, float upper_bound)
{
    if (value < lower_bound)
        return lower_bound;
    if (value > upper_bound)
        return upper_bound;

    return value;
}

// Set the fan speed between 20% min and 100% max based on chip temperature as input.
// The fan speed increases from 20% to 100% proportionally to the temperature increase from 50 and THROTTLE_TEMP
static void automatic_fan_speed(float chip_temp)
{
    double result = 0.0;
    double min_temp = 50.0;
    double min_fan_speed = 20.0;

    if (chip_temp < min_temp) {
        result = min_fan_speed;
    } else if (chip_temp >= THROTTLE_TEMP) {
        result = 100;
    } else {
        double temp_range = THROTTLE_TEMP - min_temp;
        double fan_range = 100 - min_fan_speed;
        result = ((chip_temp - min_temp) / temp_range) * fan_range + min_fan_speed;
    }

    EMC2101_set_fan_speed((float) result / 100);
}

// Returns the vcore voltage using the appropriate source
uint16_t Get_vcore(void)
{
    // TODO determine which plaform we are on for vcore retrieval

    // Regular bitaxe uses ADC for vcore
    //return ADC_get_vcore();

    // Hex regulator reports measured vcore across all 3 domains
    return (TPS546_get_vout() * 1000) / 3;
}

void POWER_MANAGEMENT_task(void * pvParameters)
{
    GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;

    PowerManagementModule * power_management = &GLOBAL_STATE->POWER_MANAGEMENT_MODULE;

    power_management->frequency_multiplier = 1;

    char * board_version = nvs_config_get_string(NVS_CONFIG_BOARD_VERSION, "unknown");
    power_management->HAS_POWER_EN =
        (strcmp(board_version, "202") == 1 || strcmp(board_version, "203") == 1 || strcmp(board_version, "204") == 1);
    power_management->HAS_PLUG_SENSE = strcmp(board_version, "204") == 1;
    free(board_version);

    int last_frequency_increase = 0;

    bool read_power = INA260_installed();

    uint16_t frequency_target = nvs_config_get_u16(NVS_CONFIG_ASIC_FREQ, CONFIG_ASIC_FREQUENCY);

    uint16_t auto_fan_speed = nvs_config_get_u16(NVS_CONFIG_AUTO_FAN_SPEED, 1);

    // Configure GPIO12 as input(barrel jack) 1 is plugged in
    gpio_config_t barrel_jack_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_12),
        .mode = GPIO_MODE_INPUT,
    };
    gpio_config(&barrel_jack_conf);
    int barrel_jack_plugged_in = gpio_get_level(GPIO_NUM_12);

    gpio_set_direction(GPIO_NUM_10, GPIO_MODE_OUTPUT);
    if (barrel_jack_plugged_in == 1 || !power_management->HAS_PLUG_SENSE) {
        // turn ASIC on
        gpio_set_level(GPIO_NUM_10, 0);
    } else {
        // turn ASIC off
        gpio_set_level(GPIO_NUM_10, 1);
    }

    vTaskDelay(3000 / portTICK_PERIOD_MS);

    while (1) {

        if (read_power == true) {
            power_management->voltage = INA260_read_voltage();
            power_management->power = INA260_read_power() / 1000;
            power_management->current = INA260_read_current();
        }
        power_management->fan_speed = EMC2101_get_fan_speed();

        if (strcmp(GLOBAL_STATE->asic_model, "BM1397") == 0) {

            power_management->chip_temp = EMC2101_get_external_temp();

            // Voltage
            // We'll throttle between 4.9v and 3.5v
            float voltage_multiplier =
                _fbound((power_management->voltage - VOLTAGE_MIN_THROTTLE) * (1 / (float) VOLTAGE_RANGE), 0, 1);

            // Temperature
            float temperature_multiplier = 1;
            float over_temp = -(THROTTLE_TEMP - power_management->chip_temp);
            if (over_temp > 0) {
                temperature_multiplier = (THROTTLE_TEMP_RANGE - over_temp) / THROTTLE_TEMP_RANGE;
            }

            float lowest_multiplier = 1;
            float multipliers[2] = {voltage_multiplier, temperature_multiplier};

            for (int i = 0; i < 2; i++) {
                if (multipliers[i] < lowest_multiplier) {
                    lowest_multiplier = multipliers[i];
                }
            }

            power_management->frequency_multiplier = lowest_multiplier;

            float target_frequency = _fbound(power_management->frequency_multiplier * frequency_target, 0, frequency_target);

            if (target_frequency < 50) {
                // TODO: Turn the chip off
            }

            // chip is coming back from a low/no voltage event
            if (power_management->frequency_value < 50 && target_frequency > 50) {
                // TODO recover gracefully?
                esp_restart();
            }

            if (power_management->frequency_value > target_frequency) {
                power_management->frequency_value = target_frequency;
                last_frequency_increase = 0;
                BM1397_send_hash_frequency(power_management->frequency_value);
                ESP_LOGI(TAG, "target %f, Freq %f, Temp %f, Power %f", target_frequency, power_management->frequency_value,
                         power_management->chip_temp, power_management->power);
            } else {
                if (last_frequency_increase > 120 && power_management->frequency_value != frequency_target) {
                    float add = (target_frequency + power_management->frequency_value) / 2;
                    power_management->frequency_value += _fbound(add, 2, 20);
                    BM1397_send_hash_frequency(power_management->frequency_value);
                    ESP_LOGI(TAG, "target %f, Freq %f, Temp %f, Power %f", target_frequency, power_management->frequency_value,
                             power_management->chip_temp, power_management->power);
                    last_frequency_increase = 60;
                } else {
                    last_frequency_increase++;
                }
            }
        } else if (strcmp(GLOBAL_STATE->asic_model, "BM1366") == 0) {
            power_management->chip_temp = EMC2101_get_internal_temp() + 5;

            if (power_management->chip_temp > THROTTLE_TEMP &&
                (power_management->frequency_value > 50 || power_management->voltage > 1000)) {
                ESP_LOGE(TAG, "OVERHEAT");


                if (power_management->HAS_POWER_EN) {
                    gpio_set_level(GPIO_NUM_10, 1);
                } else {
                    nvs_config_set_u16(NVS_CONFIG_ASIC_VOLTAGE, 990);
                    nvs_config_set_u16(NVS_CONFIG_ASIC_FREQ, 50);
                    nvs_config_set_u16(NVS_CONFIG_FAN_SPEED, 100);
                    nvs_config_set_u16(NVS_CONFIG_AUTO_FAN_SPEED, 0);
                    exit(EXIT_FAILURE);
                }

            }
        }

        if (auto_fan_speed == 1) {
            automatic_fan_speed(power_management->chip_temp);
        } else {
            EMC2101_set_fan_speed((float) nvs_config_get_u16(NVS_CONFIG_FAN_SPEED, 100) / 100);
        }
        // ESP_LOGI(TAG, "target %f, Freq %f, Volt %f, Power %f", target_frequency, power_management->frequency_value,
        // power_management->voltage, power_management->power);

        // Read the state of GPIO12
        if (power_management->HAS_PLUG_SENSE) {
            int gpio12_state = gpio_get_level(GPIO_NUM_12);
            if (gpio12_state == 0) {
                // turn ASIC off
                gpio_set_level(GPIO_NUM_10, 1);
            }
        }

        vTaskDelay(POLL_RATE / portTICK_PERIOD_MS);
    }
}


void POWER_MANAGEMENT_HEX_task(void * pvParameters)
{
    GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;

    PowerManagementModule * power_management = &GLOBAL_STATE->POWER_MANAGEMENT_MODULE;

    power_management->frequency_multiplier = 1;

    int last_frequency_increase = 0;

    uint16_t frequency_target = nvs_config_get_u16(NVS_CONFIG_ASIC_FREQ, CONFIG_ASIC_FREQUENCY);

    uint16_t auto_fan_speed = nvs_config_get_u16(NVS_CONFIG_AUTO_FAN_SPEED, 1);

    // turn on ASIC core voltage (three domains in series)
    int want_vcore = nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE, CONFIG_ASIC_VOLTAGE);
    want_vcore *= 3;  // across 3 domains
    ESP_LOGI(TAG, "---TURNING ON VCORE---");
    TPS546_set_vout(want_vcore);

    vTaskDelay(3000 / portTICK_PERIOD_MS);

    while (1) {

        // power_management members
        //uint16_t fan_speed;
        //float chip_temp;
        //float voltage;
        //float frequency_multiplier;
        //float frequency_value;
        //float power;
        //float current;

        // For reference:
        // TPS546_get_vin()- board input voltage
        //    we don't have a way to measure board input current
        // TPS546_get_vout()- core voltage *3 (across all domains)
        // TPS546_get_iout()- Current output of regulator
        //    we don't have a way to measure power, we have to calculate it
        //    but we don't have total board current, so calculate regulator power
        // TPS546_get_temperature()- gets internal regulator temperature
        // TMP1075_read_temperature(index)- gets the values from the two board sensors
        // TPS546_get_frequency()- gets the regulator switching frequency (probably no need to display)


        power_management->voltage = TPS546_get_vin() * 1000;
        power_management->current = TPS546_get_iout() * 1000;

        // calculate regulator power (in milliwatts)
        power_management->power = (TPS546_get_vout() * power_management->current) / 1000;

        // TODO fix fan driver
        //power_management->fan_speed = EMC2101_get_fan_speed();

        // Two board temperature sensors
        ESP_LOGI(TAG, "Board Temp: %d, %d", TMP1075_read_temperature(0), TMP1075_read_temperature(1));

        // get regulator internal temperature
        power_management->chip_temp = (float)TPS546_get_temperature();
        ESP_LOGI(TAG, "TPS546 Temp: %2f", power_management->chip_temp);

        // TODO figure out best way to detect overheating on the Hex
        if (power_management->chip_temp > TPS546_THROTTLE_TEMP &&
            (power_management->frequency_value > 50 || power_management->voltage > 1000)) {
            ESP_LOGE(TAG, "OVERHEAT");

            // Turn off core voltage
            TPS546_set_vout(0);

            nvs_config_set_u16(NVS_CONFIG_ASIC_VOLTAGE, 990);
            nvs_config_set_u16(NVS_CONFIG_ASIC_FREQ, 50);
            nvs_config_set_u16(NVS_CONFIG_FAN_SPEED, 100);
            nvs_config_set_u16(NVS_CONFIG_AUTO_FAN_SPEED, 0);
            exit(EXIT_FAILURE);
        }

        // TODO fix fan driver
        //if (auto_fan_speed == 1) {
        //    automatic_fan_speed(power_management->chip_temp);
        //} else {
        //    EMC2101_set_fan_speed((float) nvs_config_get_u16(NVS_CONFIG_FAN_SPEED, 100) / 100);
        //}

        ESP_LOGI(TAG, "VIN: %f, VOUT: %f, IOUT: %f", TPS546_get_vin(), TPS546_get_vout(), TPS546_get_iout());
        ESP_LOGI(TAG, "Regulator power: %f mW", power_management->power);
        ESP_LOGI(TAG, "TPS546 Frequency %d", TPS546_get_frequency());

        vTaskDelay(POLL_RATE / portTICK_PERIOD_MS);
    }
}

