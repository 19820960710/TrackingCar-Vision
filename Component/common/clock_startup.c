/**
 * @file clock_startup.c
 * @brief Bounded SYSPLL startup override for the MSPM0G3507 board.
 *
 * SysConfig emits a weak SYSCFG_DL_SYSCTL_init() which can retry the FCC
 * frequency-ratio check forever.  A failed check must not prevent motor or
 * vision tasks from starting.  This strong implementation preserves the
 * board's startup and prevents the application from being trapped before any
 * peripheral is initialized.  The first bring-up revision uses the internal
 * 32 MHz SYSOSC; the external-clock path can be restored after hardware
 * verification.
 */
#include "ti_msp_dl_config.h"

volatile uint32_t g_clock_pll_locked = 0U;

static void init_uart_115200(UART_Regs *instance)
{
    const DL_UART_Main_ClockConfig clock_config = {
        .clockSel = DL_UART_MAIN_CLOCK_BUSCLK,
        .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1,
    };
    const DL_UART_Main_Config uart_config = {
        .mode = DL_UART_MAIN_MODE_NORMAL,
        .direction = DL_UART_MAIN_DIRECTION_TX_RX,
        .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
        .parity = DL_UART_MAIN_PARITY_NONE,
        .wordLength = DL_UART_MAIN_WORD_LENGTH_8_BITS,
        .stopBits = DL_UART_MAIN_STOP_BITS_ONE,
    };

    DL_UART_Main_setClockConfig(instance, (DL_UART_Main_ClockConfig *)&clock_config);
    DL_UART_Main_init(instance, (DL_UART_Main_Config *)&uart_config);
    DL_UART_Main_setOversampling(instance, DL_UART_OVERSAMPLING_RATE_16X);
    /* 32 MHz / (16 * 115200) = 17 + 23 / 64. */
    DL_UART_Main_setBaudRateDivisor(instance, 17U, 23U);
    DL_UART_Main_enableInterrupt(instance, DL_UART_MAIN_INTERRUPT_RX);
    DL_UART_Main_enable(instance);
}

void SYSCFG_DL_SYSCTL_init(void)
{
    DL_SYSCTL_setBORThreshold(DL_SYSCTL_BOR_THRESHOLD_LEVEL_0);
    DL_SYSCTL_setFlashWaitState(DL_SYSCTL_FLASH_WAIT_STATE_0);
    DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE);
    DL_SYSCTL_disableHFXT();
    DL_SYSCTL_disableSYSPLL();
    DL_SYSCTL_setULPCLKDivider(DL_SYSCTL_ULPCLK_DIV_2);
    DL_SYSCTL_enableMFCLK();
    NVIC_SetPriority(GPIOA_INT_IRQn, 3);
}

/* Generated UART setup assumes 80 MHz.  Override it for SYSOSC 32 MHz. */
void SYSCFG_DL_UART_YAW_init(void)
{
    init_uart_115200(UART_YAW_INST);
}

void SYSCFG_DL_UART_PITCH_init(void)
{
    init_uart_115200(UART_PITCH_INST);
}

void SYSCFG_DL_UART_VISION_init(void)
{
    init_uart_115200(UART_VISION_INST);
}
