#ifndef __DO_DI_CTRL_H__
#define __DO_DI_CTRL_H__

#define	LED_STATE_CONTINUE_OFF	0//OFF
#define	LED_STATE_CONTINUE_ON	1//ON

#define	LED_STATE_BLINK_200MS	2//200ms亮，200ms灭
#define	LED_STATE_BLINK_600MS	3//600ms亮，600ms灭
#define	LED_STATE_BLINK_1000MS	4//1000ms亮，1000ms灭，循环闪烁
#define	LED_STATE_BLINK_2000MS	5//2000ms亮，2000ms灭，循环闪烁
#define	LED_STATE_BLINK_200MS_1800MS	6//200ms亮，1800ms灭，循环闪烁


void app_DO_DI_init(void);
void app_state_led_task(void);
void app_ADC_check(void);
void app_ADC_init(void);
void DRM0_Stat_Check(void);
void Button_Stat_Check(void);
void led_Start_DTU(void);

#endif

