#include <stdio.h>
#include <math.h>
#include "esp_log.h"

#include "vcore.h"
#include "adc.h"
#include "DS4432U.h"
#include "TPS546.h"

#define BITAXE_IFS     98921 // nA
#define BITAXE_RTOP    4750  // Ohm
#define BITAXE_RBOT    3320  // Ohm
#define BITAXE_VNOM    1451  // mV

static const char *TAG = "vcore.c";

void VCORE_init(GlobalState * global_state) {
    if (global_state->board_version == 402) {
        TPS546_init();
    }
    ADC_init(ADC_ATTEN_DB_11);
    ADC_ch_init(ADC_CHANNEL_1);
}

bool VCORE_set_voltage(uint16_t vcore_mv, GlobalState * global_state)
{
    uint32_t sum_vcore_mv = vcore_mv * global_state->voltage_domain;

    switch (global_state->device_model) {
        case DEVICE_MAX:
        case DEVICE_ULTRA:
        case DEVICE_SUPRA:
            if (global_state->board_version == 402) {
                ESP_LOGI(TAG, "Set ASIC voltage = %umV", vcore_mv);
                TPS546_set_vout((float)sum_vcore_mv / 1000.0);
            } else {
                uint8_t reg_setting = DS4432U_voltage_to_reg(
                    sum_vcore_mv, BITAXE_VNOM, BITAXE_RTOP, BITAXE_RBOT, BITAXE_IFS, 600);
                ESP_LOGI(TAG, "Set ASIC voltage = %umV [0x%02X]", vcore_mv, reg_setting);
                DS4432U_set_current_code(0, reg_setting); /// eek!
            }
            break;
        // case DEVICE_HEX:
        default:
    }

    return true;
}

uint16_t VCORE_get_voltage_mv(GlobalState * global_state) {
    return ADC_ch_get(ADC_CHANNEL_1) / global_state->voltage_domain;
}
