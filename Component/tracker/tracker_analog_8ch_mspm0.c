#include "tracker/tracker_analog_8ch_mspm0.h"

#include "FreeRTOS.h"
#include <stddef.h>

/*
 * clock_startup.c deliberately runs this board from the 32 MHz SYSOSC.
 * Override the weak 80 MHz SysConfig-derived ADC setup so its input range and
 * the sensor settling delay match the clock that is actually running.
 */
void SYSCFG_DL_ADC12_0_init(void)
{
    const DL_ADC12_ClockConfig clock_config = {
        .clockSel = DL_ADC12_CLOCK_ULPCLK,
        .divideRatio = DL_ADC12_CLOCK_DIVIDE_8,
        .freqRange = DL_ADC12_CLOCK_FREQ_RANGE_8_TO_16,
    };

    DL_ADC12_setClockConfig(
        ADC12_0_INST, (DL_ADC12_ClockConfig *)&clock_config);
    DL_ADC12_initSeqSample(
        ADC12_0_INST, DL_ADC12_REPEAT_MODE_ENABLED,
        DL_ADC12_SAMPLING_SOURCE_AUTO, DL_ADC12_TRIG_SRC_SOFTWARE,
        DL_ADC12_SEQ_START_ADDR_00, DL_ADC12_SEQ_END_ADDR_00,
        DL_ADC12_SAMP_CONV_RES_12_BIT, DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);
    DL_ADC12_configConversionMem(
        ADC12_0_INST, ADC12_0_ADCMEM_0, DL_ADC12_INPUT_CHAN_0,
        DL_ADC12_REFERENCE_VOLTAGE_VDDA, DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0,
        DL_ADC12_AVERAGING_MODE_DISABLED, DL_ADC12_BURN_OUT_SOURCE_DISABLED,
        DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_setSampleTime0(ADC12_0_INST, 10u);
    DL_ADC12_enableConversions(ADC12_0_INST);
}

static void write_pin(GPIO_Regs *port, uint32_t pin, uint8_t high)
{
    if (high != 0u) {
        DL_GPIO_setPins(port, pin);
    } else {
        DL_GPIO_clearPins(port, pin);
    }
}

static void set_address(void *context, uint8_t address)
{
    (void)context;
    write_pin(ADC_AD0_PORT, ADC_AD0_PIN, address & 0x01u);
    write_pin(ADC_AD1_PORT, ADC_AD1_PIN, (address >> 1u) & 0x01u);
    write_pin(ADC_AD2_PORT, ADC_AD2_PIN, (address >> 2u) & 0x01u);
}

static void delay_us_callback(void *context, uint32_t delay_us)
{
    (void)context;
    delay_cycles((configCPU_CLOCK_HZ / 1000000u) * delay_us);
}

static uint8_t read_adc(void *context, uint16_t *value)
{
    tracker_analog_8ch_mspm0_t *platform =
        (tracker_analog_8ch_mspm0_t *)context;
    uint32_t sum = 0u;
    uint8_t sample_index;

    if ((platform == NULL) || (platform->adc == NULL) || (value == NULL)) {
        return 0u;
    }
    for (sample_index = 0u;
         sample_index < TRACKER_ANALOG_8CH_ADC_SAMPLES_PER_CHANNEL;
         sample_index++) {
        uint32_t remaining = platform->timeout_iterations;
        DL_ADC12_clearInterruptStatus(
            platform->adc, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
        DL_ADC12_startConversion(platform->adc);
        while ((DL_ADC12_getRawInterruptStatus(
                    platform->adc,
                    DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED) == 0u) &&
               (remaining > 0u)) {
            remaining--;
        }
        if (remaining == 0u) {
            platform->timeout_count++;
            return 0u;
        }
        sum += DL_ADC12_getMemResult(platform->adc, platform->memory_index);
    }
    *value = (uint16_t)(sum / TRACKER_ANALOG_8CH_ADC_SAMPLES_PER_CHANNEL);
    return 1u;
}

void tracker_analog_8ch_mspm0_init(tracker_analog_8ch_mspm0_t *platform)
{
    if (platform == NULL) {
        return;
    }
    platform->adc = ADC12_0_INST;
    platform->memory_index = ADC12_0_ADCMEM_0;
    platform->timeout_iterations =
        TRACKER_ANALOG_8CH_ADC_TIMEOUT_ITERATIONS;
    platform->timeout_count = 0u;
    DL_ADC12_startConversion(platform->adc);
}

void tracker_analog_8ch_mspm0_make_hal(
    tracker_analog_8ch_mspm0_t *platform,
    tracker_analog_8ch_hal_t *hal)
{
    if (hal == NULL) {
        return;
    }
    hal->context = platform;
    hal->set_address = set_address;
    hal->delay_us = delay_us_callback;
    hal->read_adc = read_adc;
}
