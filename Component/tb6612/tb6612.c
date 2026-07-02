/**
 * TB6612 直流电机驱动模块
 * 左电机 = A（AO1/AO2），右电机 = B（BO1/BO2）
 * PWM 频率：20kHz（TIMG0, period=2000, BUSCLK=40MHz）
 * 接线：
 *   PWMA  → PA12 (TIMG0_CCP0)
 *   AIN2  → PB19
 *   AIN1  → PB17
 *   STBY  → 已接 +5V（代码不控制）
 *   BIN1  → PA16
 *   BIN2  → PB24
 *   PWMB  → PA13 (TIMG0_CCP1)
 */
#include "tb6612.h"
#include "ti_msp_dl_config.h"

/* 绝对值上限 */
#define SPEED_MAX      100
/* 周期计数 = 2000（40MHz / 2000 = 20kHz） */
#define PWM_PERIOD     2000

/* ── 方向脚操作宏 ── */
#define AIN1_PORT      GPIO_TB6612_PIN_AIN1_PORT
#define AIN1_PIN       GPIO_TB6612_PIN_AIN1_PIN
#define AIN2_PORT      GPIO_TB6612_PIN_AIN2_PORT
#define AIN2_PIN       GPIO_TB6612_PIN_AIN2_PIN
#define BIN1_PORT      GPIO_TB6612_PIN_BIN1_PORT
#define BIN1_PIN       GPIO_TB6612_PIN_BIN1_PIN
#define BIN2_PORT      GPIO_TB6612_PIN_BIN2_PORT
#define BIN2_PIN       GPIO_TB6612_PIN_BIN2_PIN

/* ── PWM 通道宏 ── */
#define PWM_C0_IDX     GPIO_PWM_TB6612_C0_IDX   /* DL_TIMER_CC_0_INDEX, 左电机 */
#define PWM_C1_IDX     GPIO_PWM_TB6612_C1_IDX   /* DL_TIMER_CC_1_INDEX, 右电机 */

/* ── 内部辅助 ── */
static inline void set_dir_a(uint32_t ain1, uint32_t ain2)
{
    if (ain1) DL_GPIO_setPins(AIN1_PORT, AIN1_PIN);
    else      DL_GPIO_clearPins(AIN1_PORT, AIN1_PIN);
    if (ain2) DL_GPIO_setPins(AIN2_PORT, AIN2_PIN);
    else      DL_GPIO_clearPins(AIN2_PORT, AIN2_PIN);
}

static inline void set_dir_b(uint32_t bin1, uint32_t bin2)
{
    if (bin1) DL_GPIO_setPins(BIN1_PORT, BIN1_PIN);
    else      DL_GPIO_clearPins(BIN1_PORT, BIN1_PIN);
    if (bin2) DL_GPIO_setPins(BIN2_PORT, BIN2_PIN);
    else      DL_GPIO_clearPins(BIN2_PORT, BIN2_PIN);
}

static void set_motor(const char motor, int16_t speed)
{
    /* 钳位 */
    if (speed > SPEED_MAX)  speed = SPEED_MAX;
    if (speed < -SPEED_MAX) speed = -SPEED_MAX;

    uint32_t abs_speed = (speed >= 0) ? (uint32_t)speed : (uint32_t)(-speed);
    /* 占空比换算：ccValue = PERIOD * (100 - abs_speed) / 100 */
    uint32_t cc_val    = PWM_PERIOD * (SPEED_MAX - abs_speed) / SPEED_MAX;

    if (motor == 'A') {
        if (speed > 0) {
            set_dir_a(1, 0);   /* AIN1=1, AIN2=0 → 前进 */
        } else if (speed < 0) {
            set_dir_a(0, 1);   /* AIN1=0, AIN2=1 → 后退 */
        } else {
            set_dir_a(0, 0);   /* 滑行停止 */
        }
        DL_TimerG_setCaptureCompareValue(PWM_TB6612_INST, cc_val, PWM_C0_IDX);
    } else {
        if (speed > 0) {
            set_dir_b(1, 0);   /* BIN1=1, BIN2=0 → 前进 */
        } else if (speed < 0) {
            set_dir_b(0, 1);   /* BIN1=0, BIN2=1 → 后退 */
        } else {
            set_dir_b(0, 0);   /* 滑行停止 */
        }
        DL_TimerG_setCaptureCompareValue(PWM_TB6612_INST, cc_val, PWM_C1_IDX);
    }
}

/* ── 公开接口 ── */

void tb6612_init(void)
{
    /* 初始化为停止（方向脚均已由 SysConfig 初始化为低电平） */
    set_dir_a(0, 0);
    set_dir_b(0, 0);

    /* PWM 占空比 0%（cc = period）已由 SYSCFG_DL_PWM_TB6612_init() 设置 */
    /* 启动 PWM 计数器 */
    DL_TimerG_startCounter(PWM_TB6612_INST);
}

void tb6612_set_left_speed(int16_t speed)
{
    set_motor('A', speed);
}

void tb6612_set_right_speed(int16_t speed)
{
    set_motor('B', speed);
}

void tb6612_set_speed(int16_t left, int16_t right)
{
    set_motor('A', left);
    set_motor('B', right);
}

void tb6612_stop(void)
{
    set_dir_a(0, 0);
    set_dir_b(0, 0);
    DL_TimerG_setCaptureCompareValue(PWM_TB6612_INST, PWM_PERIOD, PWM_C0_IDX);
    DL_TimerG_setCaptureCompareValue(PWM_TB6612_INST, PWM_PERIOD, PWM_C1_IDX);
}

void tb6612_brake(void)
{
    /* 短接制动：AIN1=AIN2=1（或 BIN1=BIN2=1） */
    set_dir_a(1, 1);
    set_dir_b(1, 1);
    DL_TimerG_setCaptureCompareValue(PWM_TB6612_INST, PWM_PERIOD, PWM_C0_IDX);
    DL_TimerG_setCaptureCompareValue(PWM_TB6612_INST, PWM_PERIOD, PWM_C1_IDX);
}
