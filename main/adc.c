#include "esp_err.h"
#include "esp_adc/adc_cali.h"

#include "adc.h"

// static const char *TAG = "adc.c";
static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc1_cali_handle;
static adc_atten_t adc_attenuation;

void ADC_init(adc_atten_t attenuation)
{
	adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = attenuation,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle));
    adc_attenuation = attenuation;
}

void ADC_ch_init(adc_channel_t channel)
{
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = adc_attenuation,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, channel, &config));
}

// returns the ADC voltage in mV
// Avoid calling from multiple threads without locks
int ADC_ch_get(adc_channel_t channel)
{
    int adc_raw_voltage, adc_voltage;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, channel, &adc_raw_voltage));
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw_voltage, &adc_voltage));
    return adc_voltage;
}
