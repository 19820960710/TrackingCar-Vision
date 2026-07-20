#ifndef TRACKER_SENSOR_ANALOG_8CH_H
#define TRACKER_SENSOR_ANALOG_8CH_H

#include <stdint.h>

#define TRACKER_ANALOG_8CH_COUNT 8u

typedef enum {
    TRACKER_ANALOG_8CH_OK = 0,
    TRACKER_ANALOG_8CH_INVALID_ARGUMENT,
    TRACKER_ANALOG_8CH_NOT_CALIBRATED,
    TRACKER_ANALOG_8CH_READ_FAILED
} tracker_analog_8ch_status_t;

typedef struct {
    void *context;
    void (*set_address)(void *context, uint8_t address);
    void (*delay_us)(void *context, uint32_t delay_us);
    uint8_t (*read_adc)(void *context, uint16_t *value);
} tracker_analog_8ch_hal_t;

typedef struct {
    uint16_t raw[TRACKER_ANALOG_8CH_COUNT];
    uint16_t normalized[TRACKER_ANALOG_8CH_COUNT];
    uint8_t digital_mask;
} tracker_analog_8ch_sample_t;

typedef struct {
    tracker_analog_8ch_hal_t hal;
    uint16_t black[TRACKER_ANALOG_8CH_COUNT];
    uint16_t white[TRACKER_ANALOG_8CH_COUNT];
    uint16_t black_threshold[TRACKER_ANALOG_8CH_COUNT];
    uint16_t white_threshold[TRACKER_ANALOG_8CH_COUNT];
    uint16_t normalization_max;
    uint8_t calibrated;
} tracker_analog_8ch_t;

void tracker_analog_8ch_init(tracker_analog_8ch_t *sensor,
                             const tracker_analog_8ch_hal_t *hal);
void tracker_analog_8ch_sample_init(tracker_analog_8ch_sample_t *sample);
tracker_analog_8ch_status_t tracker_analog_8ch_set_calibration(
    tracker_analog_8ch_t *sensor,
    const uint16_t black[TRACKER_ANALOG_8CH_COUNT],
    const uint16_t white[TRACKER_ANALOG_8CH_COUNT],
    uint16_t normalization_max);
tracker_analog_8ch_status_t tracker_analog_8ch_read(
    tracker_analog_8ch_t *sensor,
    tracker_analog_8ch_sample_t *sample);
float tracker_analog_8ch_compute_analog_error(
    const tracker_analog_8ch_t *sensor,
    const tracker_analog_8ch_sample_t *sample,
    uint8_t line_is_white,
    float error_on_no_line);
uint16_t tracker_analog_8ch_compute_peak_line_strength(
    const tracker_analog_8ch_t *sensor,
    const tracker_analog_8ch_sample_t *sample,
    uint8_t line_is_white);

#endif
