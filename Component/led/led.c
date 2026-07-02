#include "led.h"
#include "ti_msp_dl_config.h"

void led_init(void)
{
    /* GPIO 已在 SYSCFG_DL_init() 中初始化 */
}

void led_toggle(void)
{
    DL_GPIO_togglePins(LED_PORT, LED_PIN_22_PIN);
}

void led_on(void)
{
    DL_GPIO_clearPins(LED_PORT, LED_PIN_22_PIN);
}

void led_off(void)
{
    DL_GPIO_setPins(LED_PORT, LED_PIN_22_PIN);
}
