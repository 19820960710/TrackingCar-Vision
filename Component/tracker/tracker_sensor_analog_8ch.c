#include "tracker/tracker_sensor_analog_8ch.h"

#include <stddef.h>

#define TRACKER_ANALOG_8CH_SETTLE_TIME_US 10u

static uint16_t normalize_sample(uint16_t raw,
                                 uint16_t black,
                                 uint16_t white,
                                 uint16_t normalization_max)
{
    if (raw <= black) {
        return 0u;
    }
    if (raw >= white) {
        return normalization_max;
    }
    return (uint16_t)(((uint32_t)(raw - black) * normalization_max) /
                      (white - black));
}

void tracker_analog_8ch_init(tracker_analog_8ch_t *sensor,
                             const tracker_analog_8ch_hal_t *hal)
{
    uint8_t i;

    if ((sensor == NULL) || (hal == NULL)) {
        return;
    }
    sensor->hal = *hal;
    sensor->normalization_max = 0u;
    sensor->calibrated = 0u;
    for (i = 0u; i < TRACKER_ANALOG_8CH_COUNT; i++) {
        sensor->black[i] = 0u;
        sensor->white[i] = 0u;
        sensor->black_threshold[i] = 0u;
        sensor->white_threshold[i] = 0u;
    }
}

void tracker_analog_8ch_sample_init(tracker_analog_8ch_sample_t *sample)
{
    uint8_t i;

    if (sample == NULL) {
        return;
    }
    sample->digital_mask = 0u;
    for (i = 0u; i < TRACKER_ANALOG_8CH_COUNT; i++) {
        sample->raw[i] = 0u;
        sample->normalized[i] = 0u;
    }
}

tracker_analog_8ch_status_t tracker_analog_8ch_set_calibration(
    tracker_analog_8ch_t *sensor,
    const uint16_t black[TRACKER_ANALOG_8CH_COUNT],
    const uint16_t white[TRACKER_ANALOG_8CH_COUNT],
    uint16_t normalization_max)
{
    uint8_t i;

    if ((sensor == NULL) || (black == NULL) || (white == NULL) ||
        (normalization_max == 0u)) {
        return TRACKER_ANALOG_8CH_INVALID_ARGUMENT;
    }
    for (i = 0u; i < TRACKER_ANALOG_8CH_COUNT; i++) {
        uint16_t range;
        if (white[i] <= black[i]) {
            return TRACKER_ANALOG_8CH_INVALID_ARGUMENT;
        }
        range = (uint16_t)(white[i] - black[i]);
        sensor->black[i] = black[i];
        sensor->white[i] = white[i];
        sensor->black_threshold[i] = (uint16_t)(black[i] + range / 3u);
        sensor->white_threshold[i] =
            (uint16_t)(black[i] + (2u * range) / 3u);
    }
    sensor->normalization_max = normalization_max;
    sensor->calibrated = 1u;
    return TRACKER_ANALOG_8CH_OK;
}

tracker_analog_8ch_status_t tracker_analog_8ch_read(
    tracker_analog_8ch_t *sensor,
    tracker_analog_8ch_sample_t *sample)
{
    uint8_t i;

    if ((sensor == NULL) || (sample == NULL) ||
        (sensor->hal.set_address == NULL) ||
        (sensor->hal.read_adc == NULL)) {
        return TRACKER_ANALOG_8CH_INVALID_ARGUMENT;
    }
    if (sensor->calibrated == 0u) {
        return TRACKER_ANALOG_8CH_NOT_CALIBRATED;
    }

    for (i = 0u; i < TRACKER_ANALOG_8CH_COUNT; i++) {
        sensor->hal.set_address(sensor->hal.context, i);
        if (sensor->hal.delay_us != NULL) {
            sensor->hal.delay_us(sensor->hal.context,
                                 TRACKER_ANALOG_8CH_SETTLE_TIME_US);
        }
        if (sensor->hal.read_adc(sensor->hal.context, &sample->raw[i]) == 0u) {
            return TRACKER_ANALOG_8CH_READ_FAILED;
        }
        sample->normalized[i] = normalize_sample(
            sample->raw[i], sensor->black[i], sensor->white[i],
            sensor->normalization_max);
        if (sample->raw[i] > sensor->white_threshold[i]) {
            sample->digital_mask |= (uint8_t)(1u << i);
        } else if (sample->raw[i] < sensor->black_threshold[i]) {
            sample->digital_mask &= (uint8_t)~(1u << i);
        }
    }
    return TRACKER_ANALOG_8CH_OK;
}

static uint16_t line_weight(const tracker_analog_8ch_t *sensor,
                            uint16_t normalized,
                            uint8_t line_is_white)
{
    if (normalized > sensor->normalization_max) {
        normalized = sensor->normalization_max;
    }
    return (line_is_white != 0u)
               ? normalized
               : (uint16_t)(sensor->normalization_max - normalized);
}

uint16_t tracker_analog_8ch_compute_peak_line_strength(
    const tracker_analog_8ch_t *sensor,
    const tracker_analog_8ch_sample_t *sample,
    uint8_t line_is_white)
{
    uint16_t peak = 0u;
    uint8_t i;

    if ((sensor == NULL) || (sample == NULL) ||
        (sensor->calibrated == 0u) || (sensor->normalization_max == 0u)) {
        return 0u;
    }
    for (i = 0u; i < TRACKER_ANALOG_8CH_COUNT; i++) {
        uint16_t weight = line_weight(sensor, sample->normalized[i],
                                      line_is_white);
        if (weight > peak) {
            peak = weight;
        }
    }
    return peak;
}

float tracker_analog_8ch_compute_analog_error(
    const tracker_analog_8ch_t *sensor,
    const tracker_analog_8ch_sample_t *sample,
    uint8_t line_is_white,
    float error_on_no_line)
{
    uint32_t total_weight = 0u;
    float weighted_sum = 0.0f;
    uint8_t i;

    if ((sensor == NULL) || (sample == NULL) ||
        (sensor->calibrated == 0u) || (sensor->normalization_max == 0u)) {
        return error_on_no_line;
    }
    for (i = 0u; i < TRACKER_ANALOG_8CH_COUNT; i++) {
        uint16_t weight = line_weight(sensor, sample->normalized[i],
                                      line_is_white);
        total_weight += weight;
        weighted_sum += (float)weight * (float)i;
    }
    if (total_weight == 0u) {
        return error_on_no_line;
    }
    return ((weighted_sum / (float)total_weight) - 3.5f) / 3.5f;
}
