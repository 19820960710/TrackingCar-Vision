#include "app/line_tracking.h"

#include "FreeRTOS.h"
#include "task.h"
#include "config/line_tracking_config.h"
#include "service/speed_service.h"
#include "tracker/tracker_analog_8ch_mspm0.h"
#include "tracker/tracker_corner_alignment.h"
#include "tracker/tracker_corner_detector.h"
#include "tracker/tracker_position_control.h"
#include "tracker/tracker_sensor_analog_8ch.h"

#define LINE_TRACKING_TWO_PI_F 6.283185307f

typedef struct {
    tracker_analog_8ch_mspm0_t sensor_platform;
    tracker_analog_8ch_t sensor;
    tracker_analog_8ch_sample_t sample;
    tracker_position_controller_t position_controller;
    tracker_corner_detector_t corner_detector;
    tracker_corner_alignment_t corner_alignment;
    tracker_corner_alignment_output_t corner_output;
    int32_t loss_start_left_count;
    int32_t loss_start_right_count;
    uint16_t loss_elapsed_cycles;
    uint16_t reacquire_cycles;
    uint8_t line_detected;
    uint8_t has_ever_tracked;
    line_tracking_mode_t mode;
} line_tracking_context_t;

static line_tracking_context_t g_line;
volatile line_tracking_status_t g_line_tracking_status;

static const uint16_t g_black[TRACKER_ANALOG_8CH_COUNT] =
    LINE_TRACKING_BLACK_CALIBRATION;
static const uint16_t g_white[TRACKER_ANALOG_8CH_COUNT] =
    LINE_TRACKING_WHITE_CALIBRATION;
static const tracker_position_config_t g_position_config =
    LINE_TRACKING_POSITION_CONFIG;
static const tracker_corner_detector_config_t g_corner_detector_config =
    LINE_TRACKING_CORNER_DETECTOR_CONFIG;
static const tracker_corner_alignment_config_t g_corner_alignment_config =
    LINE_TRACKING_CORNER_ALIGNMENT_CONFIG;

static int32_t abs_count_delta(int32_t current, int32_t start)
{
    int32_t delta = current - start;
    return (delta < 0) ? -delta : delta;
}

static void apply_wheel_targets(float left_mm_s, float right_mm_s)
{
    g_line_tracking_status.left_target_mm_s = left_mm_s;
    g_line_tracking_status.right_target_mm_s = right_mm_s;
    (void)speed_service_set_target_mm_s(left_mm_s, right_mm_s);
}

static void stop_wheels(void)
{
    apply_wheel_targets(0.0f, 0.0f);
}

static void mirror_sample_for_debug(uint16_t peak, float line_error)
{
    uint8_t channel;

    for (channel = 0u; channel < TRACKER_ANALOG_8CH_COUNT; channel++) {
        g_line_tracking_status.raw[channel] = g_line.sample.raw[channel];
        g_line_tracking_status.normalized[channel] =
            g_line.sample.normalized[channel];
    }
    g_line_tracking_status.sample_count++;
    g_line_tracking_status.line_peak_strength = peak;
    g_line_tracking_status.line_detected = g_line.line_detected;
    g_line_tracking_status.line_error = line_error;
}

static float encoder_count_to_wheel_angle(int32_t count)
{
    return (float)count * LINE_TRACKING_TWO_PI_F /
           LINE_TRACKING_ENCODER_COUNTS_PER_REV;
}

static void build_corner_input(const speed_service_state_t *speed,
                               float line_error,
                               tracker_corner_alignment_input_t *input)
{
    *input = (tracker_corner_alignment_input_t){0};
    input->detector_event_count = g_line.corner_detector.event_count;
    input->detected_feature = g_line.corner_detector.last_feature;
    input->detected_direction = g_line.corner_detector.last_direction;
    input->candidate_feature = g_line.corner_detector.candidate_feature;
    input->candidate_direction = g_line.corner_detector.candidate_direction;
    input->candidate_frames = g_line.corner_detector.candidate_frames;
    input->line_sample_valid = 1u;
    input->line_detected = g_line.line_detected;
    input->scan_center_line_detected =
        tracker_corner_alignment_scan_center_line_detected(
            g_line.sample.normalized, g_line.sensor.normalization_max,
            g_corner_alignment_config.post_turn_scan_line_threshold);
    input->line_error = line_error;
    input->wheel_angle_rad[TRACKER_CORNER_ALIGNMENT_LEFT_WHEEL] =
        encoder_count_to_wheel_angle(speed->left_encoder_count);
    input->wheel_angle_rad[TRACKER_CORNER_ALIGNMENT_RIGHT_WHEEL] =
        encoder_count_to_wheel_angle(speed->right_encoder_count);
    input->measured_speed_mm_s[TRACKER_CORNER_ALIGNMENT_LEFT_WHEEL] =
        speed->left_speed_mm_s;
    input->measured_speed_mm_s[TRACKER_CORNER_ALIGNMENT_RIGHT_WHEEL] =
        speed->right_speed_mm_s;
}

static void update_corner_debug(void)
{
    g_line_tracking_status.corner_event_count =
        g_line.corner_detector.event_count;
    g_line_tracking_status.corner_turn_count =
        g_line.corner_alignment.turn_count;
    g_line_tracking_status.corner_abort_count =
        g_line.corner_alignment.abort_count;
}

static void enter_loss_turn(const speed_service_state_t *speed)
{
    g_line.loss_start_left_count = speed->left_encoder_count;
    g_line.loss_start_right_count = speed->right_encoder_count;
    g_line.loss_elapsed_cycles = 0u;
    g_line.reacquire_cycles = 0u;
    g_line.mode = LINE_TRACKING_MODE_LOSS_TURN_CW;
    g_line_tracking_status.loss_turn_count++;
    tracker_position_control_reset(&g_line.position_controller);
    tracker_corner_alignment_reset(
        &g_line.corner_alignment, g_line.corner_detector.event_count);

    /* Source-project convention: positive turn direction is physical right. */
    apply_wheel_targets(LINE_TRACKING_LOSS_TURN_LEFT_MM_S,
                        LINE_TRACKING_LOSS_TURN_RIGHT_MM_S);
}

static void finish_loss_turn_or_brake(void)
{
    stop_wheels();
    if (g_line.line_detected != 0u) {
        if (g_line.reacquire_cycles < UINT16_MAX) {
            g_line.reacquire_cycles++;
        }
        if (g_line.reacquire_cycles >=
            LINE_TRACKING_LOSS_REACQUIRE_CYCLES) {
            g_line.mode = LINE_TRACKING_MODE_TRACKING;
            tracker_position_control_reset(&g_line.position_controller);
        }
    } else {
        g_line.mode = LINE_TRACKING_MODE_LOSS_BRAKED;
        g_line.reacquire_cycles = 0u;
    }
}

static void update_loss_turn(const speed_service_state_t *speed)
{
    int32_t left_progress = abs_count_delta(
        speed->left_encoder_count, g_line.loss_start_left_count);
    int32_t right_progress = abs_count_delta(
        speed->right_encoder_count, g_line.loss_start_right_count);
    uint8_t left_complete =
        (uint8_t)(left_progress >= LINE_TRACKING_LOSS_TURN_90_COUNTS);
    uint8_t right_complete =
        (uint8_t)(right_progress >= LINE_TRACKING_LOSS_TURN_90_COUNTS);

    g_line_tracking_status.loss_turn_left_progress = left_progress;
    g_line_tracking_status.loss_turn_right_progress = right_progress;
    if (g_line.loss_elapsed_cycles < UINT16_MAX) {
        g_line.loss_elapsed_cycles++;
    }

    if ((left_complete != 0u) && (right_complete != 0u)) {
        finish_loss_turn_or_brake();
    } else if (g_line.loss_elapsed_cycles >=
               LINE_TRACKING_LOSS_TURN_TIMEOUT_CYCLES) {
        g_line.mode = LINE_TRACKING_MODE_LOSS_BRAKED;
        stop_wheels();
    } else {
        apply_wheel_targets(
            (left_complete != 0u) ? 0.0f :
                LINE_TRACKING_LOSS_TURN_LEFT_MM_S,
            (right_complete != 0u) ? 0.0f :
                LINE_TRACKING_LOSS_TURN_RIGHT_MM_S);
    }
}

static void update_loss_braked(float line_error)
{
    stop_wheels();
    if (g_line.line_detected == 0u) {
        g_line.reacquire_cycles = 0u;
        return;
    }
    if (g_line.reacquire_cycles < UINT16_MAX) {
        g_line.reacquire_cycles++;
    }
    if (g_line.reacquire_cycles >= LINE_TRACKING_LOSS_REACQUIRE_CYCLES) {
        g_line.mode = LINE_TRACKING_MODE_TRACKING;
        tracker_position_control_reset(&g_line.position_controller);
        tracker_position_control_update(
            &g_line.position_controller, line_error, 1u,
            LINE_TRACKING_CONTROL_PERIOD_MS / 1000.0f);
        apply_wheel_targets(g_line.position_controller.left_target_speed,
                            g_line.position_controller.right_target_speed);
    }
}

bool line_tracking_init(void)
{
    tracker_analog_8ch_hal_t hal;

    g_line = (line_tracking_context_t){0};
    g_line_tracking_status = (line_tracking_status_t){0};
    g_line.mode = LINE_TRACKING_MODE_TRACKING;
    tracker_analog_8ch_mspm0_init(&g_line.sensor_platform);
    tracker_analog_8ch_mspm0_make_hal(&g_line.sensor_platform, &hal);
    tracker_analog_8ch_init(&g_line.sensor, &hal);
    tracker_analog_8ch_sample_init(&g_line.sample);
    tracker_position_control_init(&g_line.position_controller,
                                  &g_position_config);
    tracker_corner_detector_init(&g_line.corner_detector,
                                 &g_corner_detector_config);
    if (tracker_corner_alignment_init(
            &g_line.corner_alignment, &g_corner_alignment_config, 0u) == 0u) {
        return false;
    }
    return tracker_analog_8ch_set_calibration(
               &g_line.sensor, g_black, g_white,
               LINE_TRACKING_NORMALIZATION_MAX) == TRACKER_ANALOG_8CH_OK;
}

static void process_valid_sample(void)
{
    speed_service_state_t speed = {0};
    tracker_corner_alignment_input_t corner_input;
    uint16_t peak = tracker_analog_8ch_compute_peak_line_strength(
        &g_line.sensor, &g_line.sample, LINE_TRACKING_LINE_IS_WHITE);
    float line_error = 0.0f;

    if ((g_line.line_detected == 0u) &&
        (peak >= LINE_TRACKING_PEAK_ENTER_THRESHOLD)) {
        g_line.line_detected = 1u;
    } else if ((g_line.line_detected != 0u) &&
               (peak <= LINE_TRACKING_PEAK_EXIT_THRESHOLD)) {
        g_line.line_detected = 0u;
    }
    if (g_line.line_detected != 0u) {
        line_error = tracker_analog_8ch_compute_analog_error(
            &g_line.sensor, &g_line.sample,
            LINE_TRACKING_LINE_IS_WHITE, 0.0f);
    }
    mirror_sample_for_debug(peak, line_error);
    if (!speed_service_get_state(&speed)) {
        stop_wheels();
        return;
    }

    if (g_line.mode == LINE_TRACKING_MODE_LOSS_TURN_CW) {
        update_loss_turn(&speed);
        return;
    }
    if (g_line.mode == LINE_TRACKING_MODE_LOSS_BRAKED) {
        update_loss_braked(line_error);
        return;
    }

    tracker_corner_detector_update(
        &g_line.corner_detector, g_line.sample.normalized,
        g_line.sensor.normalization_max, g_line.line_detected);
    build_corner_input(&speed, line_error, &corner_input);
    tracker_corner_alignment_update(
        &g_line.corner_alignment, &corner_input, &g_line.corner_output);
    update_corner_debug();

    if (g_line.corner_output.overriding != 0u) {
        g_line.mode = LINE_TRACKING_MODE_CORNER_ALIGNMENT;
        tracker_position_control_reset(&g_line.position_controller);
        apply_wheel_targets(g_line.corner_output.left_target_speed_mm_s,
                            g_line.corner_output.right_target_speed_mm_s);
        return;
    }

    g_line.mode = LINE_TRACKING_MODE_TRACKING;
    if (g_line.line_detected != 0u) {
        g_line.has_ever_tracked = 1u;
        tracker_position_control_update(
            &g_line.position_controller, line_error, 1u,
            LINE_TRACKING_CONTROL_PERIOD_MS / 1000.0f);
        apply_wheel_targets(g_line.position_controller.left_target_speed,
                            g_line.position_controller.right_target_speed);
    } else if (g_line.has_ever_tracked != 0u) {
        enter_loss_turn(&speed);
    } else {
        tracker_position_control_reset(&g_line.position_controller);
        stop_wheels();
    }
}

void line_tracking_task(void *pvParameters)
{
    TickType_t next_wake = xTaskGetTickCount();
    (void)pvParameters;

    for (;;) {
        tracker_analog_8ch_status_t read_status =
            tracker_analog_8ch_read(&g_line.sensor, &g_line.sample);

        if (read_status == TRACKER_ANALOG_8CH_OK) {
            process_valid_sample();
        } else {
            g_line_tracking_status.read_failure_count++;
            g_line.line_detected = 0u;
            g_line.mode = LINE_TRACKING_MODE_LOSS_BRAKED;
            tracker_position_control_reset(&g_line.position_controller);
            stop_wheels();
        }
        g_line_tracking_status.adc_timeout_count =
            g_line.sensor_platform.timeout_count;
        g_line_tracking_status.mode = g_line.mode;
        vTaskDelayUntil(&next_wake,
                        pdMS_TO_TICKS(LINE_TRACKING_CONTROL_PERIOD_MS));
    }
}
