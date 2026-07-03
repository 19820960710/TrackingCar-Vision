/**
 * 双路增量式编码器读取模块
 *
 * 实测接线映射：
 * 物理右轮：硬件 QEI，A=PA26(TIMG8_CCP0)，B=PA27(TIMG8_CCP1)
 * 物理左轮：当前接线为 A=PA25、B=PA14，TIMG12 不支持 QEI，因此用 GPIO 双边沿软件解码。
 */
#include "encoder/encoder.h"
#include "ti_msp_dl_config.h"

#define LEFT_QEI_MAX_COUNT      (65535U)
#define LEFT_QEI_HALF_RANGE     (32768)

#define RIGHT_A_PIN             GPIO_ENCODER_RIGHT_PIN_RIGHT_A_PIN
#define RIGHT_B_PIN             GPIO_ENCODER_RIGHT_PIN_RIGHT_B_PIN
#define RIGHT_PINS              (RIGHT_A_PIN | RIGHT_B_PIN)

/* 统一方向：两轮向前转时，编码器累计值均增加。当前右轮实测向前为负，因此取反。 */
#define RIGHT_ENCODER_DIR       (-1)

static int32_t g_left_count = 0;
static uint16_t g_left_last_raw = 0;
static int32_t g_left_last_report = 0;

static volatile int32_t g_right_count = 0;
static volatile uint8_t g_right_last_state = 0;
static int32_t g_right_last_report = 0;

static uint8_t read_right_state(void)
{
    uint32_t pins = DL_GPIO_readPins(GPIO_ENCODER_RIGHT_PORT, RIGHT_PINS);
    uint8_t a = (pins & RIGHT_A_PIN) ? 1U : 0U;
    uint8_t b = (pins & RIGHT_B_PIN) ? 1U : 0U;

    return (uint8_t)((a << 1) | b);
}

static void update_left_count(void)
{
    uint16_t now = (uint16_t)DL_TimerG_getTimerCount(QEI_ENCODER_LEFT_INST);
    int32_t diff = (int32_t)now - (int32_t)g_left_last_raw;

    if (diff > LEFT_QEI_HALF_RANGE) {
        diff -= (int32_t)(LEFT_QEI_MAX_COUNT + 1U);
    } else if (diff < -LEFT_QEI_HALF_RANGE) {
        diff += (int32_t)(LEFT_QEI_MAX_COUNT + 1U);
    }

    g_left_count += diff;
    g_left_last_raw = now;
}

void encoder_init(void)
{
    g_left_last_raw = (uint16_t)DL_TimerG_getTimerCount(QEI_ENCODER_LEFT_INST);
    g_left_count = 0;
    g_left_last_report = 0;

    g_right_last_state = read_right_state();
    g_right_count = 0;
    g_right_last_report = 0;

    DL_GPIO_clearInterruptStatus(GPIO_ENCODER_RIGHT_PORT, RIGHT_PINS);
    NVIC_ClearPendingIRQ(GPIO_ENCODER_RIGHT_INT_IRQN);
    NVIC_SetPriority(GPIO_ENCODER_RIGHT_INT_IRQN, 3);
    NVIC_EnableIRQ(GPIO_ENCODER_RIGHT_INT_IRQN);

    DL_TimerG_startCounter(QEI_ENCODER_LEFT_INST);
}

void encoder_reset(void)
{
    DL_TimerG_stopCounter(QEI_ENCODER_LEFT_INST);
    DL_TimerG_setTimerCount(QEI_ENCODER_LEFT_INST, 0);
    g_left_last_raw = 0;
    g_left_count = 0;
    g_left_last_report = 0;

    g_right_last_state = read_right_state();
    g_right_count = 0;
    g_right_last_report = 0;

    DL_TimerG_startCounter(QEI_ENCODER_LEFT_INST);
}

void encoder_get_data(encoder_data_t *data)
{
    if (data == 0) {
        return;
    }

    update_left_count();

    /*
     * 实测：PA26/PA27 这一路对应物理右轮，PA25/PA14 这一路对应物理左轮。
     * 因此这里交换输出映射，保证 OLED 上 L/R 与小车实际左右一致。
     */
    int32_t qei_now = g_left_count;
    int32_t gpio_now = g_right_count;

    data->left_count = gpio_now;
    data->right_count = qei_now;
    data->left_delta = gpio_now - g_right_last_report;
    data->right_delta = qei_now - g_left_last_report;

    g_left_last_report = qei_now;
    g_right_last_report = gpio_now;
}

int encoder_right_int_is_pending(void)
{
    return (DL_GPIO_getEnabledInterruptStatus(GPIO_ENCODER_RIGHT_PORT, RIGHT_PINS) != 0U);
}

void encoder_right_irq_handler(void)
{
    uint32_t pending = DL_GPIO_getEnabledInterruptStatus(GPIO_ENCODER_RIGHT_PORT, RIGHT_PINS);

    if ((pending & RIGHT_PINS) != 0U) {
        uint8_t state = read_right_state();
        uint8_t index = (uint8_t)((g_right_last_state << 2) | state);
        static const int8_t quad_table[16] = {
             0, -1,  1,  0,
             1,  0,  0, -1,
            -1,  0,  0,  1,
             0,  1, -1,  0
        };

        g_right_count += (int32_t)RIGHT_ENCODER_DIR * quad_table[index];
        g_right_last_state = state;

        DL_GPIO_clearInterruptStatus(GPIO_ENCODER_RIGHT_PORT, pending & RIGHT_PINS);
    }
}
