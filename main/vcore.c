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

static uint8_t ds4432_voltage_to_reg(uint32_t vout_mv, uint32_t vnom_mv,
                                     uint32_t ra_ohm, uint32_t rb_ohm,
                                     int32_t ifs_na, uint32_t vfb_mv)
{
    uint8_t reg;
    // Calculate current flowing though bottom resistor (Rb) in nA
    int32_t irb_na = (vfb_mv * 1000 * 1000) / rb_ohm;
    // Calculate current required through top resistor (Ra) to achieve vout in nA
    int32_t ira_na = ((vout_mv - vfb_mv) * 1000 * 1000) / ra_ohm;
    // Calculate the delta current the DAC needs to sink/source in nA
    uint32_t dac_na = abs(irb_na - ira_na);
    // Calculate required DAC steps to get dac_na (rounded)
    uint32_t dac_steps = ((dac_na * 127) + (ifs_na / 2)) / ifs_na;

    // make sure the requested voltage is in within range
    if (dac_steps > 127)
        return 0;

    reg = dac_steps;

    // dac_steps is absolute. For sink S = 0; for source S = 1.
    if (vout_mv < vnom_mv)
        reg |= 0x80;

    return reg;
}

bool VCORE_set_voltage(float core_voltage, GlobalState * global_state)
{
    switch (global_state->device_model) {
        case DEVICE_MAX:
        case DEVICE_ULTRA:
        case DEVICE_SUPRA:
            if (global_state->board_version == 402) {
                ESP_LOGI(TAG, "Set ASIC voltage = %.3fV", core_voltage);
                TPS546_set_vout(core_voltage * (float)global_state->voltage_domain);
            } else {
                uint16_t vcore_mv = core_voltage * 1000 * global_state->voltage_domain;
                uint8_t reg_setting = ds4432_voltage_to_reg(
                    vcore_mv, BITAXE_VNOM, BITAXE_RTOP, BITAXE_RBOT, BITAXE_IFS, 600);
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
