/*
 * Copyright (c) 2023, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ============ ti_msp_dl_config.c =============
 *  Configured MSPM0 DriverLib module definitions
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */

#include "ti_msp_dl_config.h"

DL_UART_Main_backupConfig gmaxicamBackup;

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform any initialization needed before using any board APIs
 */
SYSCONFIG_WEAK void SYSCFG_DL_init(void)
{
    SYSCFG_DL_initPower();
    SYSCFG_DL_GPIO_init();
    /* Module-Specific Initializations*/
    SYSCFG_DL_SYSCTL_init();
    SYSCFG_DL_stepMotor1_init();
    SYSCFG_DL_stepMotor2_init();
    SYSCFG_DL_maxicam_init();
    /* Ensure backup structures have no valid state */
	gmaxicamBackup.backupRdy 	= false;

}
/*
 * User should take care to save and restore register configuration in application.
 * See Retention Configuration section for more details.
 */
SYSCONFIG_WEAK bool SYSCFG_DL_saveConfiguration(void)
{
    bool retStatus = true;

	retStatus &= DL_UART_Main_saveConfiguration(maxicam_INST, &gmaxicamBackup);

    return retStatus;
}


SYSCONFIG_WEAK bool SYSCFG_DL_restoreConfiguration(void)
{
    bool retStatus = true;

	retStatus &= DL_UART_Main_restoreConfiguration(maxicam_INST, &gmaxicamBackup);

    return retStatus;
}

SYSCONFIG_WEAK void SYSCFG_DL_initPower(void)
{
    DL_GPIO_reset(GPIOA);
    DL_GPIO_reset(GPIOB);
    DL_UART_Main_reset(stepMotor1_INST);
    DL_UART_Main_reset(stepMotor2_INST);
    DL_UART_Main_reset(maxicam_INST);

    DL_GPIO_enablePower(GPIOA);
    DL_GPIO_enablePower(GPIOB);
    DL_UART_Main_enablePower(stepMotor1_INST);
    DL_UART_Main_enablePower(stepMotor2_INST);
    DL_UART_Main_enablePower(maxicam_INST);
    delay_cycles(POWER_STARTUP_DELAY);
}

SYSCONFIG_WEAK void SYSCFG_DL_GPIO_init(void)
{

    
	DL_GPIO_initPeripheralOutputFunction(
		 GPIO_stepMotor1_IOMUX_TX, GPIO_stepMotor1_IOMUX_TX_FUNC);
	DL_GPIO_initPeripheralInputFunctionFeatures(
		 GPIO_stepMotor1_IOMUX_RX, GPIO_stepMotor1_IOMUX_RX_FUNC,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initPeripheralOutputFunction(
        GPIO_stepMotor2_IOMUX_TX, GPIO_stepMotor2_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(
        GPIO_stepMotor2_IOMUX_RX, GPIO_stepMotor2_IOMUX_RX_FUNC);
    DL_GPIO_initPeripheralOutputFunction(
        GPIO_maxicam_IOMUX_TX, GPIO_maxicam_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(
        GPIO_maxicam_IOMUX_RX, GPIO_maxicam_IOMUX_RX_FUNC);

    DL_GPIO_initDigitalOutput(GPIO_GRP_0_LED_1_IOMUX);

    DL_GPIO_clearPins(GPIO_GRP_0_PORT, GPIO_GRP_0_LED_1_PIN);
    DL_GPIO_enableOutput(GPIO_GRP_0_PORT, GPIO_GRP_0_LED_1_PIN);

}


static const DL_SYSCTL_SYSPLLConfig gSYSPLLConfig = {
    .inputFreq              = DL_SYSCTL_SYSPLL_INPUT_FREQ_32_48_MHZ,
	.rDivClk2x              = 1,
	.rDivClk1               = 0,
	.rDivClk0               = 0,
	.enableCLK2x            = DL_SYSCTL_SYSPLL_CLK2X_DISABLE,
	.enableCLK1             = DL_SYSCTL_SYSPLL_CLK1_DISABLE,
	.enableCLK0             = DL_SYSCTL_SYSPLL_CLK0_ENABLE,
	.sysPLLMCLK             = DL_SYSCTL_SYSPLL_MCLK_CLK0,
	.sysPLLRef              = DL_SYSCTL_SYSPLL_REF_SYSOSC,
	.qDiv                   = 4,
	.pDiv                   = DL_SYSCTL_SYSPLL_PDIV_1
};
SYSCONFIG_WEAK void SYSCFG_DL_SYSCTL_init(void)
{

	//Low Power Mode is configured to be SLEEP0
    DL_SYSCTL_setBORThreshold(DL_SYSCTL_BOR_THRESHOLD_LEVEL_0);
    DL_SYSCTL_setFlashWaitState(DL_SYSCTL_FLASH_WAIT_STATE_2);

    
	DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE);
	/* Set default configuration */
	DL_SYSCTL_disableHFXT();
	DL_SYSCTL_disableSYSPLL();
    DL_SYSCTL_configSYSPLL((DL_SYSCTL_SYSPLLConfig *) &gSYSPLLConfig);
    DL_SYSCTL_setULPCLKDivider(DL_SYSCTL_ULPCLK_DIV_2);
    DL_SYSCTL_enableMFCLK();
    DL_SYSCTL_setMCLKSource(SYSOSC, HSCLK, DL_SYSCTL_HSCLK_SOURCE_SYSPLL);

}


static const DL_UART_Main_ClockConfig gstepMotor1ClockConfig = {
    .clockSel    = DL_UART_MAIN_CLOCK_MFCLK,
    .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1
};

static const DL_UART_Main_Config gstepMotor1Config = {
    .mode        = DL_UART_MAIN_MODE_NORMAL,
    .direction   = DL_UART_MAIN_DIRECTION_TX_RX,
    .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
    .parity      = DL_UART_MAIN_PARITY_NONE,
    .wordLength  = DL_UART_MAIN_WORD_LENGTH_8_BITS,
    .stopBits    = DL_UART_MAIN_STOP_BITS_ONE
};

SYSCONFIG_WEAK void SYSCFG_DL_stepMotor1_init(void)
{
    DL_UART_Main_setClockConfig(stepMotor1_INST, (DL_UART_Main_ClockConfig *) &gstepMotor1ClockConfig);

    DL_UART_Main_init(stepMotor1_INST, (DL_UART_Main_Config *) &gstepMotor1Config);
    /*
     * Configure baud rate by setting oversampling and baud rate divisors.
     *  Target baud rate: 115200
     *  Actual baud rate: 115107.91
     */
    DL_UART_Main_setOversampling(stepMotor1_INST, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(stepMotor1_INST, stepMotor1_IBRD_4_MHZ_115200_BAUD, stepMotor1_FBRD_4_MHZ_115200_BAUD);


    /* Configure FIFOs */
    DL_UART_Main_enableFIFOs(stepMotor1_INST);
    DL_UART_Main_setRXFIFOThreshold(stepMotor1_INST, DL_UART_RX_FIFO_LEVEL_1_2_FULL);
    DL_UART_Main_setTXFIFOThreshold(stepMotor1_INST, DL_UART_TX_FIFO_LEVEL_1_2_EMPTY);

    DL_UART_Main_enable(stepMotor1_INST);
}
static const DL_UART_Main_ClockConfig gstepMotor2ClockConfig = {
    .clockSel    = DL_UART_MAIN_CLOCK_MFCLK,
    .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1
};

static const DL_UART_Main_Config gstepMotor2Config = {
    .mode        = DL_UART_MAIN_MODE_NORMAL,
    .direction   = DL_UART_MAIN_DIRECTION_TX_RX,
    .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
    .parity      = DL_UART_MAIN_PARITY_NONE,
    .wordLength  = DL_UART_MAIN_WORD_LENGTH_8_BITS,
    .stopBits    = DL_UART_MAIN_STOP_BITS_ONE
};

SYSCONFIG_WEAK void SYSCFG_DL_stepMotor2_init(void)
{
    DL_UART_Main_setClockConfig(stepMotor2_INST, (DL_UART_Main_ClockConfig *) &gstepMotor2ClockConfig);

    DL_UART_Main_init(stepMotor2_INST, (DL_UART_Main_Config *) &gstepMotor2Config);
    /*
     * Configure baud rate by setting oversampling and baud rate divisors.
     *  Target baud rate: 115200
     *  Actual baud rate: 115107.91
     */
    DL_UART_Main_setOversampling(stepMotor2_INST, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(stepMotor2_INST, stepMotor2_IBRD_4_MHZ_115200_BAUD, stepMotor2_FBRD_4_MHZ_115200_BAUD);


    /* Configure FIFOs */
    DL_UART_Main_enableFIFOs(stepMotor2_INST);
    DL_UART_Main_setRXFIFOThreshold(stepMotor2_INST, DL_UART_RX_FIFO_LEVEL_1_2_FULL);
    DL_UART_Main_setTXFIFOThreshold(stepMotor2_INST, DL_UART_TX_FIFO_LEVEL_1_2_EMPTY);

    DL_UART_Main_enable(stepMotor2_INST);
}
static const DL_UART_Main_ClockConfig gmaxicamClockConfig = {
    .clockSel    = DL_UART_MAIN_CLOCK_BUSCLK,
    .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1
};

static const DL_UART_Main_Config gmaxicamConfig = {
    .mode        = DL_UART_MAIN_MODE_NORMAL,
    .direction   = DL_UART_MAIN_DIRECTION_TX_RX,
    .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
    .parity      = DL_UART_MAIN_PARITY_NONE,
    .wordLength  = DL_UART_MAIN_WORD_LENGTH_8_BITS,
    .stopBits    = DL_UART_MAIN_STOP_BITS_ONE
};

SYSCONFIG_WEAK void SYSCFG_DL_maxicam_init(void)
{
    DL_UART_Main_setClockConfig(maxicam_INST, (DL_UART_Main_ClockConfig *) &gmaxicamClockConfig);

    DL_UART_Main_init(maxicam_INST, (DL_UART_Main_Config *) &gmaxicamConfig);
    /*
     * Configure baud rate by setting oversampling and baud rate divisors.
     *  Target baud rate: 115200
     *  Actual baud rate: 115190.78
     */
    DL_UART_Main_setOversampling(maxicam_INST, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(maxicam_INST, maxicam_IBRD_80_MHZ_115200_BAUD, maxicam_FBRD_80_MHZ_115200_BAUD);


    /* Configure Interrupts */
    DL_UART_Main_enableInterrupt(maxicam_INST,
                                 DL_UART_MAIN_INTERRUPT_RX |
                                 DL_UART_MAIN_INTERRUPT_RX_TIMEOUT_ERROR);

    /* Configure FIFOs */
    DL_UART_Main_enableFIFOs(maxicam_INST);
    DL_UART_Main_setRXFIFOThreshold(maxicam_INST, DL_UART_RX_FIFO_LEVEL_ONE_ENTRY);
    DL_UART_Main_setTXFIFOThreshold(maxicam_INST, DL_UART_TX_FIFO_LEVEL_1_2_EMPTY);

    DL_UART_Main_setRXInterruptTimeout(maxicam_INST, 15);

    DL_UART_Main_enable(maxicam_INST);
}

