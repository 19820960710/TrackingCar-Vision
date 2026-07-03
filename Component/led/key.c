/**
 * 按键模块 - PB21 用户按键（按下低电平，内部上拉）
 * 软件消抖：连续两次读到有效电平才确认。
 */
#include "key.h"
#include "ti_msp_dl_config.h"

#define KEY_PORT   GPIO_KEY_USER_PORT
#define KEY_PIN    GPIO_KEY_USER_PIN_KEY_USER_PIN

static bool last_raw = false;
static bool debounced = false;

void key_init(void)
{
    last_raw  = false;
    debounced = false;
}

bool key_read_user(void)
{
    bool raw = (DL_GPIO_readPins(KEY_PORT, KEY_PIN) == 0);
    if (raw == last_raw) {
        debounced = raw;
    }
    last_raw = raw;
    return debounced;
}
