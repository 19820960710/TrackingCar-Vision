/**
 * M0_Templant_FreeRTOS - MSPM0G3507 FreeRTOS 工程模板
 * main.c: 胶水层，硬件初始化 + 创建 FreeRTOS 任务
 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "ti_msp_dl_config.h"
#include "led/led.h"
#include "led/key.h"
#include "UART/uart0.h"
#include "oled/oled.h"
#include "mpu6050/mpu6050.h"
#include "tb6612/tb6612.h"
#include "encoder/encoder.h"
#include "pid/pid.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int status;
    float pitch10;
    float roll10;
    float yaw10;
} attitude_msg_t;

#define ENCODER_SPEED_PERIOD_MS     10
#define TELEMETRY_PERIOD_MS         100
#define PID_DEFAULT_KP_MILLI        300     /* 调试初始值：0.300 */
#define PID_DEFAULT_KI_MILLI        20      /* 调试初始值：0.020 */
#define PID_DEFAULT_KD_MILLI        0
#define PID_OUTPUT_MIN              (-50)
#define PID_OUTPUT_MAX              (50)
#define SPEED_CMD_QUEUE_LEN         8
#define UART_CMD_LINE_MAX           64

typedef struct {
    uint32_t seq;
    uint32_t t_ms;
    int32_t target_rpm;
    int32_t left_rpm;
    int32_t right_rpm;
    int32_t left_pwm;
    int32_t right_pwm;
    int32_t kp_milli;
    int32_t ki_milli;
    int32_t kd_milli;
    uint8_t estop;
    uint8_t enabled;
} speed_status_msg_t;

typedef enum {
    CTRL_CMD_SET_SPEED = 0,
    CTRL_CMD_SET_PID,
    CTRL_CMD_STOP,
    CTRL_CMD_ESTOP,
    CTRL_CMD_CLEAR_ESTOP
} control_cmd_type_t;

typedef struct {
    control_cmd_type_t type;
    int32_t a;
    int32_t b;
    int32_t c;
} control_cmd_t;

static QueueHandle_t g_oled_status_queue = NULL;
static QueueHandle_t g_telemetry_status_queue = NULL;
static QueueHandle_t g_control_cmd_queue = NULL;
static QueueHandle_t g_attitude_queue = NULL;
static TaskHandle_t g_mpu_task_handle = NULL;
static TaskHandle_t g_speed_loop_task_handle = NULL;

static int32_t encoder_delta_to_rpm10_by_period(int32_t delta, uint32_t period_ms)
{
    if (period_ms == 0U) {
        return 0;
    }
    return (int32_t)(((int64_t)delta * 600000) /
                     ((int64_t)ENCODER_COUNTS_PER_REV * period_ms));
}

static char *skip_spaces(char *p)
{
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return p;
}

static void upper_string(char *s)
{
    while (*s) {
        if (*s >= 'a' && *s <= 'z') {
            *s = (char)(*s - 'a' + 'A');
        }
        s++;
    }
}

static bool parse_i32(char **pp, int32_t *out)
{
    char *p = skip_spaces(*pp);
    int32_t sign = 1;
    int32_t value = 0;
    bool has_digit = false;

    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+') {
        p++;
    }

    while (*p >= '0' && *p <= '9') {
        has_digit = true;
        value = value * 10 + (*p - '0');
        p++;
    }

    if (!has_digit) {
        return false;
    }

    *out = sign * value;
    *pp = p;
    return true;
}

static bool parse_gain_milli(char **pp, int32_t *out)
{
    char *p = skip_spaces(*pp);
    int32_t sign = 1;
    int32_t whole = 0;
    int32_t frac = 0;
    int32_t frac_scale = 100;
    bool has_digit = false;

    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+') {
        p++;
    }

    while (*p >= '0' && *p <= '9') {
        has_digit = true;
        whole = whole * 10 + (*p - '0');
        p++;
    }

    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9' && frac_scale > 0) {
            has_digit = true;
            frac += (*p - '0') * frac_scale;
            frac_scale /= 10;
            p++;
        }
        while (*p >= '0' && *p <= '9') {
            p++;
        }
    }

    if (!has_digit) {
        return false;
    }

    *out = sign * (whole * 1000 + frac);
    *pp = p;
    return true;
}

static void send_control_cmd(control_cmd_type_t type, int32_t a, int32_t b, int32_t c)
{
    control_cmd_t cmd;
    cmd.type = type;
    cmd.a = a;
    cmd.b = b;
    cmd.c = c;

    if (g_control_cmd_queue != NULL) {
        (void)xQueueSend(g_control_cmd_queue, &cmd, 0);
    }
}

static void uart_send_event(const char *event)
{
    char buf[80];
    snprintf(buf, sizeof(buf), "EVT %s t=%lu\r\n", event, (unsigned long)xTaskGetTickCount());
    uart0_sendStr(buf);
}

static void led_task(void *pvParameters)
{
    (void)pvParameters;
    while (1) {
        led_toggle();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void mpu_task(void *pvParameters)
{
    (void)pvParameters;
    attitude_msg_t msg = {0};

    vTaskDelay(pdMS_TO_TICKS(200));

    msg.status = MPU6050_Init();
    if (msg.status != 0) {
        xQueueOverwrite(g_attitude_queue, &msg);
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    (void)ulTaskNotifyTake(pdTRUE, 0);

    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (Read_Quad() == 0) {
            msg.status = 0;
            msg.pitch10 = pitch;
            msg.roll10 = roll;
            msg.yaw10 = yaw;
            xQueueOverwrite(g_attitude_queue, &msg);
        }
    }
}

static void apply_stop(pid_inc_t *left_pid, pid_inc_t *right_pid, speed_status_msg_t *status)
{
    tb6612_stop();
    pid_inc_reset(left_pid);
    pid_inc_reset(right_pid);
    status->target_rpm = 0;
    status->left_pwm = 0;
    status->right_pwm = 0;
    status->enabled = 0;
}

static void process_control_cmds(pid_inc_t *left_pid, pid_inc_t *right_pid, speed_status_msg_t *status)
{
    control_cmd_t cmd;

    while (xQueueReceive(g_control_cmd_queue, &cmd, 0) == pdPASS) {
        switch (cmd.type) {
        case CTRL_CMD_SET_SPEED:
            if (status->estop) {
                uart0_sendStr("ERR ESTOP_LATCHED USE CLR\r\n");
            } else {
                status->target_rpm = cmd.a;
                status->enabled = (cmd.a != 0) ? 1U : 0U;
                pid_inc_reset(left_pid);
                pid_inc_reset(right_pid);
                if (cmd.a == 0) {
                    tb6612_stop();
                    status->left_pwm = 0;
                    status->right_pwm = 0;
                }
                uart_send_event("SPD");
            }
            break;
        case CTRL_CMD_SET_PID:
            if (cmd.a < 0 || cmd.a > 5000 || cmd.b < 0 || cmd.b > 5000 || cmd.c < 0 || cmd.c > 5000) {
                uart0_sendStr("ERR PID_RANGE 0.000..5.000\r\n");
            } else {
                pid_inc_set_gain(left_pid, cmd.a, cmd.b, cmd.c);
                pid_inc_set_gain(right_pid, cmd.a, cmd.b, cmd.c);
                pid_inc_reset(left_pid);
                pid_inc_reset(right_pid);
                status->kp_milli = cmd.a;
                status->ki_milli = cmd.b;
                status->kd_milli = cmd.c;
                uart_send_event("PID");
            }
            break;
        case CTRL_CMD_STOP:
            status->estop = 0;
            apply_stop(left_pid, right_pid, status);
            uart_send_event("STOP");
            break;
        case CTRL_CMD_ESTOP:
            status->estop = 1;
            apply_stop(left_pid, right_pid, status);
            uart_send_event("ESTOP");
            break;
        case CTRL_CMD_CLEAR_ESTOP:
            status->estop = 0;
            apply_stop(left_pid, right_pid, status);
            uart_send_event("CLR");
            break;
        default:
            break;
        }
    }
}

static void send_telemetry(const speed_status_msg_t *status)
{
    char buf[192];
    snprintf(buf, sizeof(buf),
             "TEL seq=%lu t=%lu estop=%u en=%u tgt=%ld l=%ld r=%ld lp=%ld rp=%ld kp=%ld ki=%ld kd=%ld\r\n",
             (unsigned long)status->seq,
             (unsigned long)status->t_ms,
             (unsigned int)status->estop,
             (unsigned int)status->enabled,
             (long)status->target_rpm,
             (long)status->left_rpm,
             (long)status->right_rpm,
             (long)status->left_pwm,
             (long)status->right_pwm,
             (long)status->kp_milli,
             (long)status->ki_milli,
             (long)status->kd_milli);
    uart0_sendStr(buf);
}

static void speed_loop_task(void *pvParameters)
{
    (void)pvParameters;
    pid_inc_t left_pid;
    pid_inc_t right_pid;
    speed_status_msg_t status = {0};
    int32_t left_rpm10_filt = 0;
    int32_t right_rpm10_filt = 0;
    int32_t left_delta_sum = 0;
    int32_t right_delta_sum = 0;
    uint32_t speed_sample_count = 0;


    pid_inc_init(&left_pid, PID_DEFAULT_KP_MILLI, PID_DEFAULT_KI_MILLI, PID_DEFAULT_KD_MILLI,
                 PID_OUTPUT_MIN, PID_OUTPUT_MAX);
    pid_inc_init(&right_pid, PID_DEFAULT_KP_MILLI, PID_DEFAULT_KI_MILLI, PID_DEFAULT_KD_MILLI,
                 PID_OUTPUT_MIN, PID_OUTPUT_MAX);

    status.kp_milli = PID_DEFAULT_KP_MILLI;
    status.ki_milli = PID_DEFAULT_KI_MILLI;
    status.kd_milli = PID_DEFAULT_KD_MILLI;

    encoder_reset();
    xQueueOverwrite(g_oled_status_queue, &status);
    xQueueOverwrite(g_telemetry_status_queue, &status);

    NVIC_SetPriority(TIMER_0_INST_INT_IRQN, 3);
    NVIC_ClearPendingIRQ(TIMER_0_INST_INT_IRQN);
    NVIC_EnableIRQ(TIMER_0_INST_INT_IRQN);
    DL_TimerG_startCounter(TIMER_0_INST);

    for (;;) {
        encoder_data_t encoder;
        bool speed_updated = false;

        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        process_control_cmds(&left_pid, &right_pid, &status);

        encoder_get_data(&encoder);
        left_delta_sum += encoder.left_delta;
        right_delta_sum += encoder.right_delta;
        speed_sample_count++;

        /*
         * 编码器仍然 10ms 固定读取；低速测速使用 50ms 窗口。
         * PID 也只在新速度窗口完成时更新一次，避免 10ms 重复使用旧速度导致积分过快。
         */
        if (speed_sample_count >= 5U) {
            int32_t left_rpm10 = encoder_delta_to_rpm10_by_period(
                left_delta_sum, speed_sample_count * ENCODER_SPEED_PERIOD_MS);
            int32_t right_rpm10 = encoder_delta_to_rpm10_by_period(
                right_delta_sum, speed_sample_count * ENCODER_SPEED_PERIOD_MS);

            left_rpm10_filt += (left_rpm10 - left_rpm10_filt) / 2;
            right_rpm10_filt += (right_rpm10 - right_rpm10_filt) / 2;
            left_delta_sum = 0;
            right_delta_sum = 0;
            speed_sample_count = 0;
            speed_updated = true;
        }

        status.seq++;
        status.t_ms = (uint32_t)xTaskGetTickCount();
        status.left_rpm = left_rpm10_filt / 10;
        status.right_rpm = right_rpm10_filt / 10;

        if (status.estop || !status.enabled) {
            tb6612_stop();
            status.left_pwm = 0;
            status.right_pwm = 0;
        } else if (speed_updated) {
            status.left_pwm = pid_inc_compute(&left_pid, status.target_rpm, status.left_rpm);
            status.right_pwm = pid_inc_compute(&right_pid, status.target_rpm, status.right_rpm);
            /* 实测电机通道与物理左右相反：TB6612 A 控制物理右轮，B 控制物理左轮。 */
            tb6612_set_speed((int16_t)status.right_pwm, (int16_t)status.left_pwm);
        }

        xQueueOverwrite(g_oled_status_queue, &status);
        xQueueOverwrite(g_telemetry_status_queue, &status);
    }
}

static void telemetry_task(void *pvParameters)
{
    (void)pvParameters;
    speed_status_msg_t status = {0};

    for (;;) {
        speed_status_msg_t new_status;
        if (xQueueReceive(g_telemetry_status_queue, &new_status, pdMS_TO_TICKS(TELEMETRY_PERIOD_MS)) == pdPASS) {
            status = new_status;
        }
        send_telemetry(&status);
        vTaskDelay(pdMS_TO_TICKS(TELEMETRY_PERIOD_MS));
    }
}

static void estop_task(void *pvParameters)
{
    (void)pvParameters;
    bool key_was = false;

    for (;;) {
        bool key_now = key_read_user();
        if (key_now && !key_was) {
            /* 调试阶段按键只作为紧急停止，先直接停电机，再通知闭环任务锁存 ESTOP。 */
            tb6612_stop();
            send_control_cmd(CTRL_CMD_ESTOP, 0, 0, 0);
        }
        key_was = key_now;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void uart_command_task(void *pvParameters)
{
    (void)pvParameters;
    char line[UART_CMD_LINE_MAX];
    uint32_t pos = 0;
    uint8_t byte;

    uart0_sendStr("CMD READY: SPD <rpm>, PID <kp> <ki> <kd>, STOP, CLR, ESTOP, HELP\r\n");

    for (;;) {
        if (!uart0_recvByte(&byte, 50)) {
            continue;
        }

        if (byte == '\r' || byte == '\n') {
            if (pos == 0) {
                continue;
            }
            line[pos] = '\0';
            pos = 0;
            upper_string(line);

            char *p = line;
            char cmd[8] = {0};
            uint32_t i = 0;
            p = skip_spaces(p);
            while (*p != '\0' && *p != ' ' && *p != '\t' && i < sizeof(cmd) - 1U) {
                cmd[i++] = *p++;
            }
            cmd[i] = '\0';

            if (strcmp(cmd, "SPD") == 0) {
                int32_t rpm;
                if (parse_i32(&p, &rpm)) {
                    send_control_cmd(CTRL_CMD_SET_SPEED, rpm, 0, 0);
                    uart0_sendStr("OK SPD\r\n");
                } else {
                    uart0_sendStr("ERR SPD_USAGE SPD <rpm>\r\n");
                }
            } else if (strcmp(cmd, "PID") == 0) {
                int32_t kp, ki, kd;
                if (parse_gain_milli(&p, &kp) && parse_gain_milli(&p, &ki) && parse_gain_milli(&p, &kd)) {
                    send_control_cmd(CTRL_CMD_SET_PID, kp, ki, kd);
                    uart0_sendStr("OK PID\r\n");
                } else {
                    uart0_sendStr("ERR PID_USAGE PID <kp> <ki> <kd>  e.g. PID 0.300 0.020 0\r\n");
                }
            } else if (strcmp(cmd, "PIDM") == 0) {
                int32_t kp, ki, kd;
                if (parse_i32(&p, &kp) && parse_i32(&p, &ki) && parse_i32(&p, &kd)) {
                    send_control_cmd(CTRL_CMD_SET_PID, kp, ki, kd);
                    uart0_sendStr("OK PIDM\r\n");
                } else {
                    uart0_sendStr("ERR PIDM_USAGE PIDM <kp_m> <ki_m> <kd_m>  e.g. PIDM 300 20 0\r\n");
                }
            } else if (strcmp(cmd, "STOP") == 0) {
                send_control_cmd(CTRL_CMD_STOP, 0, 0, 0);
                uart0_sendStr("OK STOP\r\n");
            } else if (strcmp(cmd, "CLR") == 0 || strcmp(cmd, "START") == 0) {
                send_control_cmd(CTRL_CMD_CLEAR_ESTOP, 0, 0, 0);
                uart0_sendStr("OK CLR\r\n");
            } else if (strcmp(cmd, "ESTOP") == 0) {
                tb6612_stop();
                send_control_cmd(CTRL_CMD_ESTOP, 0, 0, 0);
                uart0_sendStr("OK ESTOP\r\n");
            } else if (strcmp(cmd, "HELP") == 0) {
                uart0_sendStr("CMD: SPD <rpm> | PID <kp> <ki> <kd> | PIDM <kp_m> <ki_m> <kd_m> | STOP | CLR | ESTOP\r\n");
                uart0_sendStr("STEP: SPD 0, SPD 10, SPD 20, SPD -10, SPD -20\r\n");
            } else {
                uart0_sendStr("ERR UNKNOWN_CMD USE HELP\r\n");
            }
        } else if (pos < UART_CMD_LINE_MAX - 1U) {
            line[pos++] = (char)byte;
        } else {
            pos = 0;
            uart0_sendStr("ERR LINE_TOO_LONG\r\n");
        }
    }
}

static void oled_task(void *pvParameters)
{
    (void)pvParameters;
    speed_status_msg_t status = {0};

    OLED_Init();
    OLED_Clear();

    for (;;) {
        speed_status_msg_t new_status;
        if (xQueueReceive(g_oled_status_queue, &new_status, 0) == pdPASS) {
            status = new_status;
        }

        OLED_Clear();
        if (status.estop) {
            OLED_ShowString(0, 0, "ESTOP", 16, 1);
        } else if (status.enabled) {
            OLED_ShowString(0, 0, "CLOSED LOOP", 16, 1);
        } else {
            OLED_ShowString(0, 0, "STOP", 16, 1);
        }
        OLED_vsprint(0, 16, 16, "T:%ld rpm", (long)status.target_rpm);
        OLED_vsprint(0, 32, 16, "L:%ld/%ld", (long)status.left_rpm, (long)status.left_pwm);
        OLED_vsprint(0, 48, 16, "R:%ld/%ld", (long)status.right_rpm, (long)status.right_pwm);
        OLED_Refresh();

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void prvSetupHardware(void)
{
    SYSCFG_DL_init();

    MPU6050_IntEnable();

    led_init();
    key_init();
    uart0_init();
    tb6612_init();
    encoder_init();
}

int main(void)
{
    prvSetupHardware();

    uart0_sendStr("M0 Speed Closed Loop Ready | UART0 COM19 | 80MHz\r\n");

    g_attitude_queue = xQueueCreate(1, sizeof(attitude_msg_t));
    g_oled_status_queue = xQueueCreate(1, sizeof(speed_status_msg_t));
    g_telemetry_status_queue = xQueueCreate(1, sizeof(speed_status_msg_t));
    g_control_cmd_queue = xQueueCreate(SPEED_CMD_QUEUE_LEN, sizeof(control_cmd_t));
    if (g_attitude_queue == NULL || g_oled_status_queue == NULL ||
        g_telemetry_status_queue == NULL || g_control_cmd_queue == NULL) {
        while (1) {}
    }

    xTaskCreate(led_task,          "LED",      128, NULL, 1, NULL);
    xTaskCreate(mpu_task,          "MPU",      512, NULL, 2, &g_mpu_task_handle);
    xTaskCreate(speed_loop_task,   "SPD_LOOP", 384, NULL, 3, &g_speed_loop_task_handle);
    xTaskCreate(estop_task,        "ESTOP",    192, NULL, 3, NULL);
    xTaskCreate(uart_command_task, "UART_CMD", 384, NULL, 2, NULL);
    xTaskCreate(telemetry_task,    "TEL",      384, NULL, 1, NULL);
    xTaskCreate(oled_task,         "OLED",     512, NULL, 1, NULL);

    vTaskStartScheduler();

    while (1) {}
}

/* 中断服务函数 */
void GROUP1_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (encoder_right_int_is_pending()) {
        encoder_right_irq_handler();
    }

    if (MPU6050_IntIsPending()) {
        MPU6050_IntClear();
        if (g_mpu_task_handle != NULL) {
            vTaskNotifyGiveFromISR(g_mpu_task_handle, &xHigherPriorityTaskWoken);
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void TIMER_0_INST_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    switch (DL_TimerG_getPendingInterrupt(TIMER_0_INST)) {
    case DL_TIMER_IIDX_ZERO:
        if (g_speed_loop_task_handle != NULL) {
            vTaskNotifyGiveFromISR(g_speed_loop_task_handle, &xHigherPriorityTaskWoken);
        }
        break;
    default:
        break;
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* FreeRTOS 钩子 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask; (void)pcTaskName;
    while (1) {}
}

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
    StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize)
{
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
    StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize)
{
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
