#ifndef POWER_MANAGEMENT_TASK_H_
#define POWER_MANAGEMENT_TASK_H_

typedef struct
{
    uint16_t fan_perc;
    uint16_t fan_rpm;
    float chip_temp[6];
    float chip_temp_avg;
    float vr_temp;
    int voltage_mv;
    float frequency_multiplier;
    float frequency_value;
    int power_mw;
    int current_ma;
    bool HAS_POWER_EN;
    bool HAS_PLUG_SENSE;
} PowerManagementModule;

void POWER_MANAGEMENT_task(void * pvParameters);

#endif
