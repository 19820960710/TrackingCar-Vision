#include "clock.h"

#include "FreeRTOS.h"
#include "task.h"
#include "ti_msp_dl_config.h"

int mspm0_delay_ms(unsigned long num_ms)
{
    if (num_ms == 0U) {
        return 0;
    }

    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        vTaskDelay(pdMS_TO_TICKS(num_ms));
    } else {
        while (num_ms--) {
            delay_cycles(CPUCLK_FREQ / 1000U);
        }
    }
    return 0;
}

int mspm0_get_clock_ms(unsigned long *count)
{
    if (count == NULL) {
        return 1;
    }

    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        *count = (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    } else {
        *count = 0U;
    }
    return 0;
}

void SysTick_Init(void)
{
}
