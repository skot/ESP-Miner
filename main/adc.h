#ifndef ADC_H_
#define ADC_H_

#ifdef __cplusplus
extern "C"
{
#endif

void ADC_init(void);
uint16_t ADC_get_vcore(void);

#ifdef __cplusplus
}
#endif

#endif /* ADC_H_ */