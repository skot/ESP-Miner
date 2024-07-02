#ifndef ADC_H_
#define ADC_H_

#include "esp_adc/adc_oneshot.h"

void ADC_init(adc_atten_t attenuation);
void ADC_ch_init(adc_channel_t channel);
int ADC_ch_get(adc_channel_t channel);

#endif /* ADC_H_ */
