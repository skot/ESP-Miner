#ifndef LED_CONTROLLER_H_
#define LED_CONTROLLER_H_

#ifdef __cplusplus
extern "C" {
#endif

void initLEDs(void);
void ledc_init(void);
void led_set(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_CONTROLLER_H_ */