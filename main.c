#include "ti_msp_dl_config.h"

#include "pitch_motor_control.h"

#define PITCH_MOTOR_SELF_TEST_ENABLED (0U)

static pitch_motor_t g_yaw_motor;   /* UART2 / PA23 */
static pitch_motor_t g_pitch_motor; /* UART1 / PB6  */

int main(void)
{
    SYSCFG_DL_init();
    pitch_motor_init(&g_yaw_motor,   stepMotor1_INST, 1U);
    pitch_motor_init(&g_pitch_motor, stepMotor2_INST, 1U);
    pitch_motor_enable(&g_pitch_motor); /* PB6 first — guaranteed to work */
    pitch_motor_enable(&g_yaw_motor);   /* PA23 second */

    while (1) {
        pitch_motor_enable(&g_pitch_motor);
        pitch_motor_enable(&g_yaw_motor);
        pitch_motor_poll(&g_pitch_motor);
        pitch_motor_poll(&g_yaw_motor);

#if PITCH_MOTOR_SELF_TEST_ENABLED
        pitch_motor_run_self_test(&g_yaw_motor);
        pitch_motor_run_self_test(&g_pitch_motor);
#endif
        delay_cycles(CPUCLK_FREQ);
    }
}
