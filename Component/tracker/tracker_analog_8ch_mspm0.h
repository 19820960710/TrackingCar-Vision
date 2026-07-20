#ifndef TRACKER_ANALOG_8CH_MSPM0_H
#define TRACKER_ANALOG_8CH_MSPM0_H

#include <stdint.h>

#include "ti_msp_dl_config.h"
#include "tracker/tracker_sensor_analog_8ch.h"

#define TRACKER_ANALOG_8CH_ADC_TIMEOUT_ITERATIONS 100000u
#define TRACKER_ANALOG_8CH_ADC_SAMPLES_PER_CHANNEL 8u

/* SysConfig emits one shared port macro when all address pins use GPIOB. */
#if !defined(ADC_AD0_PORT) && defined(ADC_PORT)
#define ADC_AD0_PORT ADC_PORT
#define ADC_AD1_PORT ADC_PORT
#define ADC_AD2_PORT ADC_PORT
#endif

typedef struct {
    ADC12_Regs *adc;
    DL_ADC12_MEM_IDX memory_index;
    uint32_t timeout_iterations;
    uint32_t timeout_count;
} tracker_analog_8ch_mspm0_t;

void tracker_analog_8ch_mspm0_init(tracker_analog_8ch_mspm0_t *platform);
void tracker_analog_8ch_mspm0_make_hal(
    tracker_analog_8ch_mspm0_t *platform,
    tracker_analog_8ch_hal_t *hal);

#endif
