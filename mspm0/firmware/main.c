/*
 * Gimbal communication bring-up.
 * MaixCAM UART4 connects to MSPM0 UART3: PB2=TX, PB3=RX.
 * Yaw motor connects to UART2: PA23=TX, PA24=RX.
 * Pitch motor connects to UART1: PB6=TX, PB7=RX.
 */
#include "ti_msp_dl_config.h"

#include "gimbal_motor.h"
#include "vision_config.h"
#include "vision_uart.h"

#include <stdbool.h>
#include <stdint.h>

#define APP_HEARTBEAT_PERIOD_MS (500U)
#define GIMBAL_UART_DIAGNOSTIC_PERIOD_MS (1000U)
#define GIMBAL_UART_DIAGNOSTIC_ENABLED     (1U)

/*
 * The X42S motor itself closes its position loop with its internal encoder.
 * Keep this set to 0 while checking wiring and the mechanical travel.  Change
 * it to 1 only to run the small, yaw-only motion test described below.
 */
#define GIMBAL_MOTOR_SELF_TEST_ENABLED       (0U)
#define GIMBAL_MOTOR_SELF_TEST_START_DELAY_MS (500U)
#define GIMBAL_MOTOR_SELF_TEST_PULSES         (160)
#define GIMBAL_MOTOR_SELF_TEST_SPEED_RPM      (60U)
#define GIMBAL_MOTOR_SELF_TEST_ACCELERATION   (20U)
#define GIMBAL_MOTOR_SELF_TEST_WAIT_MS        (1000U)

#if GIMBAL_MOTOR_SELF_TEST_ENABLED
typedef enum {
    GIMBAL_SELF_TEST_WAIT_START,
    GIMBAL_SELF_TEST_MOVE_POSITIVE,
    GIMBAL_SELF_TEST_MOVE_NEGATIVE,
    GIMBAL_SELF_TEST_STOPPED,
    GIMBAL_SELF_TEST_COMPLETE,
} gimbal_self_test_state_t;
#endif

static volatile uint32_t g_monotonic_ms;
static gimbal_motor_t g_yaw_motor;
static gimbal_motor_t g_pitch_motor;

static const vision_uart_config_t g_maixcam_uart = {
    .instance = maxicam_INST,
    .irqn = maxicam_INST_INT_IRQN,
    .frame_width = VISION_DEFAULT_FRAME_WIDTH,
    .frame_height = VISION_DEFAULT_FRAME_HEIGHT,
};

static const gimbal_motor_config_t g_yaw_motor_config = {
    .uart = stepMotor1_INST,
    .address = 1U,
    .positive_is_cw = true,
};

static const gimbal_motor_config_t g_pitch_motor_config = {
    .uart = stepMotor2_INST,
    .address = 1U,
    .positive_is_cw = true,
};

static uint32_t app_monotonic_ms(void)
{
    return g_monotonic_ms;
}

/* Temporary bring-up indication: LED1 must toggle every 500 ms. */
static void app_heartbeat_update(uint32_t now_ms)
{
    static uint32_t last_toggle_ms;

    if ((uint32_t) (now_ms - last_toggle_ms) >= APP_HEARTBEAT_PERIOD_MS) {
        DL_GPIO_togglePins(GPIO_GRP_0_PORT, GPIO_GRP_0_LED_1_PIN);
        last_toggle_ms = now_ms;
    }
}

/*
 * Temporary, non-motion UART test. It sends the EMM V5 01 35 6B speed-query
 * frame through the yaw motor wrapper, so the output is UART2/PA23.
 */
static void gimbal_uart_diagnostic_update(uint32_t now_ms)
{
#if GIMBAL_UART_DIAGNOSTIC_ENABLED
    static uint32_t last_request_ms;

    if ((uint32_t) (now_ms - last_request_ms) >=
        GIMBAL_UART_DIAGNOSTIC_PERIOD_MS) {
        (void) gimbal_motor_request_speed(&g_yaw_motor);
        last_request_ms = now_ms;
    }
#else
    (void) now_ms;
#endif
}

void SysTick_Handler(void)
{
    g_monotonic_ms++;
}

void maxicam_INST_IRQHandler(void)
{
    vision_uart_on_uart_irq();
}

/*
 * A deliberately small commissioning test.  At 16 microsteps, 160 pulses is
 * approximately 18 degrees.  It exercises only yaw so pitch remains still.
 */
static void gimbal_motor_self_test_update(uint32_t now_ms)
{
#if GIMBAL_MOTOR_SELF_TEST_ENABLED
    static gimbal_self_test_state_t state = GIMBAL_SELF_TEST_WAIT_START;
    static uint32_t state_start_ms;

    #define TIME_ELAPSED(now, start, duration) \
        ((uint32_t) ((now) - (start)) >= (duration))

    switch (state) {
    case GIMBAL_SELF_TEST_WAIT_START:
        if (TIME_ELAPSED(now_ms, state_start_ms,
                         GIMBAL_MOTOR_SELF_TEST_START_DELAY_MS)) {
            (void) gimbal_motor_set_enabled(&g_yaw_motor, true);
            state = GIMBAL_SELF_TEST_MOVE_POSITIVE;
            state_start_ms = now_ms;
        }
        break;

    case GIMBAL_SELF_TEST_MOVE_POSITIVE:
        if (TIME_ELAPSED(now_ms, state_start_ms, 100U)) {
            (void) gimbal_motor_move_relative(
                &g_yaw_motor, GIMBAL_MOTOR_SELF_TEST_PULSES,
                GIMBAL_MOTOR_SELF_TEST_SPEED_RPM,
                GIMBAL_MOTOR_SELF_TEST_ACCELERATION);
            state = GIMBAL_SELF_TEST_MOVE_NEGATIVE;
            state_start_ms = now_ms;
        }
        break;

    case GIMBAL_SELF_TEST_MOVE_NEGATIVE:
        if (TIME_ELAPSED(now_ms, state_start_ms,
                         GIMBAL_MOTOR_SELF_TEST_WAIT_MS)) {
            (void) gimbal_motor_move_relative(
                &g_yaw_motor, -GIMBAL_MOTOR_SELF_TEST_PULSES,
                GIMBAL_MOTOR_SELF_TEST_SPEED_RPM,
                GIMBAL_MOTOR_SELF_TEST_ACCELERATION);
            state = GIMBAL_SELF_TEST_STOPPED;
            state_start_ms = now_ms;
        }
        break;

    case GIMBAL_SELF_TEST_STOPPED:
        if (TIME_ELAPSED(now_ms, state_start_ms,
                         GIMBAL_MOTOR_SELF_TEST_WAIT_MS)) {
            (void) gimbal_motor_stop(&g_yaw_motor);
            (void) gimbal_motor_set_enabled(&g_yaw_motor, false);
            state = GIMBAL_SELF_TEST_COMPLETE;
            state_start_ms = now_ms;
        }
        break;

    case GIMBAL_SELF_TEST_COMPLETE:
        break;

    default:
        state = GIMBAL_SELF_TEST_STOPPED;
        break;
    }
    #undef TIME_ELAPSED
#else
    (void) now_ms;
#endif
}

int main(void)
{
    SYSCFG_DL_init();
    (void) SysTick_Config(CPUCLK_FREQ / 1000U);
    (void) vision_uart_init(&g_maixcam_uart);
    (void) gimbal_motor_init(&g_yaw_motor, &g_yaw_motor_config);
    (void) gimbal_motor_init(&g_pitch_motor, &g_pitch_motor_config);
    DL_GPIO_clearPins(GPIO_GRP_0_PORT, GPIO_GRP_0_LED_1_PIN);

    while (1) {
        uint32_t now_ms = app_monotonic_ms();

        app_heartbeat_update(now_ms);
        gimbal_uart_diagnostic_update(now_ms);
        gimbal_motor_self_test_update(now_ms);
        (void) vision_uart_process(now_ms);
    }
}
