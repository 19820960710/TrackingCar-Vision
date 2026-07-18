/* ============================================================================
 *   闲鱼定制 小研分享屋
 *   任何非闲鱼小研分享屋出售的均为盗版
 *   正式比赛代码绑定机器绑定芯片，任何二手出售均无效
 *   请认准正版
 * ============================================================================ */

/**
 * @file    app_tasks.c
 * @brief   FreeRTOS 任务入口、ISR 分发与应用层 API 转发。
 *
 * @details 本文件是应用层与 service/control 层之间的胶水层，职责：
 *          1. 创建所有 FreeRTOS 任务并启动调度器；
 *          2. 保存任务句柄，承担 ISR -> 任务通知的分发；
 *          3. 对外暴露统一的 app_tasks_* API，转发到对应 service，
 *             外部调用方无需感知 service 模块名与队列句柄。
 *
 *          分层约定（详见 docs/工程总结.md 第 4 节）：
 *          app_tasks -> service(FreeRTOS 适配) -> control(纯算法) -> pid(通用库)
 *
 *          控制链路节拍（10ms Timer 统一驱动）：
 *          TIMER_0 -> yaw_loop_task -> speed_loop_task
 *          yaw 先更新目标，再通知速度环执行，避免时序错位。
 */

#include "FreeRTOS.h"
#include "task.h"
#include "ti_msp_dl_config.h"
#include "task/app_tasks.h"
#include "led/led.h"
#include "led/key.h"
#include "oled/oled.h"
#include "icm20602/icm20602.h"
#include "encoder/encoder.h"
#include "service/speed_service.h"
#include "common/i2c_bus.h"     /* i2c0_irq_handler (I2C 异步读中断) */
#include "service/attitude_service.h"
#include "service/yaw_loop_service.h"
#include "service/stepper_service.h"
#include "common/util.h"
#include "config/motor_speed_profiles.h"
#include "config/actuator_validation_config.h"
#include "config/board_feature_config.h"
#include "app/actuator_validation.h"
#include "config/vision_tracking_config.h"
#include "app/vision_tracking.h"
#include "vision/vision_uart.h"
#include <stdint.h>
#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  任务句柄：只保存需要被 ISR 通知的任务，其余任务句柄不保留。
 * ═══════════════════════════════════════════════════════════════════════════ */
static TaskHandle_t g_attitude_task_handle = NULL;   /* ICM20602/I2C 完成通知 */
static TaskHandle_t g_yaw_loop_task_handle = NULL;    /* TIMER_0 10ms 节拍通知 */
static TaskHandle_t g_speed_loop_task_handle = NULL;  /* 由 yaw_loop_task 通知 */

/* ═══════════════════════════════════════════════════════════════════════════
 *  对外控制 API：转发到 service 层，不启用 yaw 专用低速前馈。
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  设置左右轮目标线速度 (mm/s)。
 * @param  left_mm_s  左轮目标 mm/s（正数=前进，负数=后退，0=停止）
 * @param  right_mm_s 右轮目标 mm/s
 * @return true=目标已成功写入队列，false=队列未就绪
 * @note   本接口直接转发到以 mm/s 为唯一公共单位的 speed_service。
 */
bool app_tasks_set_wheel_speed_target_mm_s(float left_mm_s, float right_mm_s)
{
    return speed_service_set_target_mm_s(left_mm_s, right_mm_s);
}

/**
 * @brief  设置 yaw 闭环目标。
 * @param  base_speed_rpm   基准前进速度 (RPM)，差速转向时作为中值
 * @param  target_yaw_deg10 目标偏航角 ×10（例如 45°=450），内部会自动归一化到 ±180°
 * @return true=目标已写入队列
 */
bool app_tasks_set_yaw_target(int32_t base_speed_rpm, int32_t target_yaw_deg10)
{
    return yaw_loop_service_set_target(base_speed_rpm, target_yaw_deg10);
}

bool app_tasks_set_stepper_enabled(bool enabled)
{
    return app_tasks_set_stepper_axis_enabled(APP_STEPPER_AXIS_YAW, enabled);
}

bool app_tasks_move_stepper(const app_stepper_move_t *move)
{
    return app_tasks_move_stepper_axis(APP_STEPPER_AXIS_YAW, move);
}

static stepper_axis_t map_stepper_axis(app_stepper_axis_t axis)
{
    return (axis == APP_STEPPER_AXIS_PITCH) ? STEPPER_AXIS_PITCH :
                                              STEPPER_AXIS_YAW;
}

bool app_tasks_set_stepper_axis_enabled(app_stepper_axis_t axis, bool enabled)
{
    if ((axis != APP_STEPPER_AXIS_YAW) &&
        (axis != APP_STEPPER_AXIS_PITCH)) {
        return false;
    }
    return stepper_service_set_axis_enabled(map_stepper_axis(axis), enabled);
}

bool app_tasks_move_stepper_axis(app_stepper_axis_t axis,
                                 const app_stepper_move_t *move)
{
    stepper_motor_move_t command;

    if ((move == NULL) ||
        ((axis != APP_STEPPER_AXIS_YAW) &&
         (axis != APP_STEPPER_AXIS_PITCH))) {
        return false;
    }
    command.direction = (move->direction == APP_STEPPER_DIRECTION_CCW) ?
                        ZDT_X42S_DIRECTION_CCW : ZDT_X42S_DIRECTION_CW;
    command.speed_rpm = move->speed_rpm;
    command.acceleration = move->acceleration;
    command.pulse_count = move->pulse_count;
    command.motion_mode = move->motion_mode;
    command.sync_flag = move->sync_flag;
    return stepper_service_move_axis(map_stepper_axis(axis), &command);
}

bool app_tasks_get_stepper_state(app_stepper_state_t *out)
{
    return app_tasks_get_stepper_axis_state(APP_STEPPER_AXIS_YAW, out);
}

bool app_tasks_get_stepper_axis_state(app_stepper_axis_t axis,
                                      app_stepper_state_t *out)
{
    stepper_service_state_t state;

    if ((out == NULL) ||
        ((axis != APP_STEPPER_AXIS_YAW) &&
         (axis != APP_STEPPER_AXIS_PITCH)) ||
        !stepper_service_get_axis_state(map_stepper_axis(axis), &state)) {
        return false;
    }
    out->enabled = state.enabled;
    out->command_pending = state.command_pending;
    out->last_tx_ok = state.last_tx_ok;
    out->transmitted_commands = state.transmitted_commands;
    out->last_response = (uint8_t)state.last_response;
    return true;
}

bool app_tasks_stepper_axis_motion_reached(app_stepper_axis_t axis)
{
    stepper_service_state_t state;

    if ((axis != APP_STEPPER_AXIS_YAW) &&
        (axis != APP_STEPPER_AXIS_PITCH)) {
        return false;
    }
    return stepper_service_get_axis_state(map_stepper_axis(axis), &state) &&
           (state.last_response == ZDT_X42S_RESPONSE_REACHED);
}

/**
 * @brief  等待 yaw 到位且两轮停止。
 *         组合 yaw_loop_service_is_settled() 与 speed_service_wheels_stopped_snapshot()
 *         两个条件，不要求速度环写 yaw 状态。
 * @param  timeout_ms  等待超时（ms）
 *                     0 = 非阻塞轮询（只检查一次立即返回）
 * @return 0 = 已到位或超时结束；1 = 未到位仍在等待（仅 timeout_ms==0 时返回）
 * @note   超时返回 0 并非"成功到位"，而是"放弃等待"。调用方需检查状态区分。
 */
int app_tasks_wait_yaw_settled(uint32_t timeout_ms)
{
    TickType_t start_tick = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    for (;;) {
        /* 同时满足：yaw 进入到位锁存 + 两轮 PWM/设定点全部归零 */
        bool done = yaw_loop_service_is_settled() &&
                    speed_service_wheels_stopped_snapshot();

        if (done) {
            return 0;                   /* 到位成功 */
        }
        if (timeout_ms == 0U) {
            return 1;                   /* 非阻塞模式，未到位 */
        }
        if ((xTaskGetTickCount() - start_tick) >= timeout_ticks) {
            return 0;                   /* 超时，放弃等待 */
        }
        vTaskDelay(pdMS_TO_TICKS(10));  /* 每 10ms 轮询一次 */
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  对外状态快照 API：只读 getter，不暴露 PID/PWM/队列等内部状态。
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  获取最新姿态快照（只读 peek，非破坏性读取）。
 * @param  out  输出缓冲区，非空
 * @return true=成功读到有效或无效姿态；false=队列为空或指针无效
 */
bool app_tasks_get_attitude(app_attitude_t *out)
{
    return attitude_service_get(out);
}

/**
 * @brief  获取两轮速度快照。
 *         将 service 内部结构体 speed_service_state_t 映射为应用层 app_wheel_speed_t。
 * @param  out  输出缓冲区，非空
 * @return true=成功获取
 */
bool app_tasks_get_wheel_speed(app_wheel_speed_t *out)
{
    speed_service_state_t speed_state;

    if (out == NULL || !speed_service_get_state(&speed_state)) {
        return false;
    }

    /* 只暴露对外接口所需字段，隐藏 PWM/PID 等实现细节 */
    out->left_speed_mm_s = speed_state.left_speed_mm_s;
    out->right_speed_mm_s = speed_state.right_speed_mm_s;
    out->left_target_mm_s = speed_state.left_target_mm_s;
    out->right_target_mm_s = speed_state.right_target_mm_s;
    out->stopped = speed_state.stopped;
    return true;
}

/**
 * @brief  获取 yaw 闭环状态快照。
 * @param  out  输出缓冲区，非空
 * @return true=成功获取
 */
bool app_tasks_get_yaw_status(app_yaw_status_t *out)
{
    return yaw_loop_service_get_status(out);
}

/**
 * @brief  一次性读取车辆聚合状态：姿态 + 轮速 + yaw 状态。
 *         任一模块有数据即返回 true，调用方通过 xxx_available 字段判断具体哪些有效。
 * @param  out  输出缓冲区，非空；先清零再填充
 * @return true=至少一个模块有数据可用
 */
bool app_tasks_get_vehicle_state(app_vehicle_state_t *out)
{
    app_vehicle_state_t snapshot = {0};

    if (out == NULL) {
        return false;
    }

    *out = snapshot;
    out->attitude_available = app_tasks_get_attitude(&out->attitude);
    out->wheel_speed_available = app_tasks_get_wheel_speed(&out->wheel_speed);
    out->yaw_status_available = app_tasks_get_yaw_status(&out->yaw);
    out->stepper_available = app_tasks_get_stepper_state(&out->stepper);
    return (out->attitude_available || out->wheel_speed_available ||
            out->yaw_status_available || out->stepper_available);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  FreeRTOS 任务入口
 *
 *  控制类任务（attitude/yaw_loop/speed_loop）只做等待/延时/调用 service step；
 *  按键与 OLED 任务逻辑保留在本文件，便于频繁修改。
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  LED 心跳任务（最低优先级 1）。
 *         简单 500ms 翻转一次 GPIO，作为系统运行指示。
 */
static void led_task(void *pvParameters)
{
    (void)pvParameters;
    for (;;) {
        led_toggle();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/**
 * @brief  姿态任务（优先级 2）：ICM20602 1kHz 异步读取 + Mahony 解算。
 *
 * 流程：
 *   1. 启动异步 I2C 读取（icm20602_async_start，非阻塞，~500µs 后台传输）
 *   2. 阻塞等待 I2C 完成中断通知（I2C_0_INST_IRQHandler）
 *   3. 检查完成标志 → 解算 → 调用 attitude_service_process_sample()
 *   4. 回到步骤 1
 *
 * 传感器配为 1kHz 输出（SMPLRT_DIV=0），任务以 I2C 完成通知为节拍（~1kHz）。
 * 复位后 attitude_service 内部做 ~20s 稳定检测，稳定前不发布有效姿态。
 */
static void attitude_task(void *pvParameters)
{
    (void)pvParameters;

    /* 延迟 200ms 等外设（时钟/GPIO/I2C）稳定后再初始化 ICM20602，
     * 避免上电时序不一致导致 I2C 通信失败。 */
    vTaskDelay(pdMS_TO_TICKS(200));
    int init_ret = icm20602_init();
    if (init_ret != 0) {
        /* 初始化失败：发布一条无效姿态到队列（下游 yaw 环可感知），然后挂起。
         * 挂起而非重试，避免 I2C 总线持续报错浪费 CPU。 */
        attitude_service_publish_invalid();
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    /* init 用阻塞同步读完成配置；之后才使能 I2C 中断进入异步模式。
     * 顺序很重要：先 init（用阻塞读不会触发 ISR），再开中断，
     * 避免 i2c0_init() 的 reset 清零 IMASK 寄存器，也避免阻塞读被 ISR 干扰。 */
    i2c0_enable_int();

    attitude_service_reset();
    (void)ulTaskNotifyTake(pdTRUE, 0);   /* 清除启动期间可能残留的通知位 */
    for (;;) {
        /* 启动 I2C 异步 DMA 读取（非阻塞，立即返回）。
         * 如果总线忙（上次传输未完成），等 1ms 后重试。 */
        if (icm20602_async_start() != 0) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        /* 阻塞等待 I2C 完成中断通知。超时 2ms 作为兑底保护：
         * 如果中断丢失，不会永远死等。 */
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2));

        /* 传输完成 → 解算 Mahony 姿态 → 喂给 service 做稳定检测 */
        if (icm20602_async_is_complete()) {
            icm_attitude_t att;
            if (icm20602_async_finish(&att) == 0) {
                attitude_service_process_sample(att.pitch, att.roll, att.yaw);
            }
        }
    }
}

/**
 * @brief  yaw 角环任务（优先级 4，最高控制优先级）。
 *
 * 由 TIMER_0 10ms 中断通知唤醒，内部做 5 分频，每 50ms 执行一次 yaw PID。
 * yaw 环暂时计算 base_speed + turn_rpm；胶水层完成差速与 RPM->mm/s 换算，
 * 留在本胶水层完成，使 yaw 和 speed 两环可独立测试/调试。
 *
 * 每次被唤醒后都通知 speed_loop_task，保证速度环在 yaw 更新后紧随执行。
 */
static void yaw_loop_task(void *pvParameters)
{
    (void)pvParameters;

    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /* yaw_loop_service_step_10ms() 内部做 5 分频，
         * 返回 true 表示本周期（50ms）完成了 yaw PID 计算，
         * 此时 base_speed、turn_rpm、speed_ff_enable 等已写入状态快照。 */
        bool control_ran = yaw_loop_service_step_10ms();

        if (control_ran) {
            app_yaw_status_t yaw = {0};
            if (yaw_loop_service_get_status(&yaw) && yaw.enabled) {
                if (!yaw.attitude_valid) {
                    /* 姿态失效（MPU 未稳定或通信丢失）：安全停车 */
                    (void)speed_service_set_target_mm_s(0.0f, 0.0f);
                } else {
                    /* 差速换算：左轮 = 基准 - 转向量，右轮 = 基准 + 转向量。
                     * 实测左正右负等效于左轮减速右轮加速，小车顺时针旋转。
                     * 若实际旋转方向相反，调换 ± 号即可。 */
                    int32_t left_cmd_rpm = yaw.base_speed_rpm - yaw.turn_rpm;
                    int32_t right_cmd_rpm = yaw.base_speed_rpm + yaw.turn_rpm;

                    /* yaw 环暂时保留原 RPM 参数；只在跨入速度内环时显式换算。
                     * 后续重调 yaw 环时可将其整体迁移为 mm/s。 */
                    (void)speed_service_set_target_mm_s(
                        motor_speed_profile_rpm_to_mm_s(
                            MOTOR_SPEED_ACTIVE_PROFILE, (float)left_cmd_rpm),
                        motor_speed_profile_rpm_to_mm_s(
                            MOTOR_SPEED_ACTIVE_PROFILE, (float)right_cmd_rpm));
                }
            }
        }

        /* 每次被唤醒都通知 speed_loop_task，即使本周期 yaw 未执行 PID。
         * speed 环内部分频逻辑自行确保 50ms 执行一次 PID。 */
        if (g_speed_loop_task_handle != NULL) {
            xTaskNotifyGive(g_speed_loop_task_handle);
        }
    }
}

/**
 * @brief  速度环任务（优先级 3）。
 *
 * 负责启动 TIMER_0（10ms 硬件定时器），之后由 yaw_loop_task 的通知驱动。
 * 每次被唤醒调用 speed_service_step_10ms()，内部：
 *   1. 读编码器增量
 *   2. 累计到 30ms 窗口执行 PI
 *   3. 输出逻辑 PWM 计数到 TB6612
 */
static void speed_loop_task(void *pvParameters)
{
    (void)pvParameters;

    /* 设置 TIMER_0 中断优先级为 3（与任务优先级 3 匹配，满足 FreeRTOS
     * "逻辑优先级不高于 configMAX_SYSCALL_INTERRUPT_PRIORITY" 的要求）。 */
    NVIC_SetPriority(TIMER_0_INST_INT_IRQN, 3);
    NVIC_ClearPendingIRQ(TIMER_0_INST_INT_IRQN);
    NVIC_EnableIRQ(TIMER_0_INST_INT_IRQN);
    DL_TimerG_startCounter(TIMER_0_INST);

    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        speed_service_step_10ms();
    }
}

/**
 * @brief  按键任务（优先级 2）：PB21 按键切档 yaw 目标角。
 *
 * 每按一次切换到下一档位，顺序：
 *   0° → 45° → 90° → 135° → 180° → 0°（循环）
 * 基准速度沿用当前 yaw 状态中的值，避免切档时丢掉行走速度。
 */
static void yaw_key_task(void *pvParameters)
{
    (void)pvParameters;
    bool key_was = false;         /* 上次按键状态，用于下降沿检测 */
    int32_t yaw_deg10 = 0;       /* 当前累计目标角，循环 0°~180° */

    for (;;) {
        bool key_now = key_read_user();

        /* 下降沿检测：按键按下时为低电平（内部上拉），
         * key_now=true 表示按下，key_was=false 表示上次未按。 */
        if (key_now && !key_was) {
            int32_t base_speed_rpm = 0;
            app_yaw_status_t state = {0};

            /* 每次按键 +45°，超过 180° 回到 0° */
            yaw_deg10 += 450;            /* +45° 对应 450 个 0.1° */
            if (yaw_deg10 > 1800) {      /* 超过 180° 回到 0° */
                yaw_deg10 = 0;
            }

            /* 从当前 yaw 状态中读取 base_speed_rpm，保持已有前进速度 */
            if (app_tasks_get_yaw_status(&state)) {
                base_speed_rpm = state.base_speed_rpm;
            }
            (void)app_tasks_set_yaw_target(base_speed_rpm, yaw_deg10);
        }

        key_was = key_now;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief  OLED 公用行：绘制 yaw 目标行 "YT: ±XXX.X ON/OFF"。
 *         复用于等待 MPU 稳定和正常显示两种状态。
 * @param  yaw_state  yaw 状态快照（非空）
 */
static void oled_print_yaw_target(const app_yaw_status_t *yaw_state)
{
    int32_t tgt_abs = util_abs_i32(yaw_state->target_yaw_deg10);
    char tgt_sign = (yaw_state->target_yaw_deg10 < 0) ? '-' : ' ';

    OLED_vsprint(0, 32, 16, "YT:%c%3ld.%1ld %s", tgt_sign,
                 (long)(tgt_abs / 10), (long)(tgt_abs % 10),
                 yaw_state->enabled ? "ON " : "OFF");
}

/**
 * @brief  OLED 显示任务（最低优先级 1，软件 I2C 刷屏耗时，不宜占用高优先级）。
 *
 * 三种显示状态：
 *   1. MPU 稳定中（attitude_seen=false）：提示 "ICM stabilizing"，等待约 20s
 *   2. MPU 失效（valid=false）：提示 "icm failure"，检查硬件
 *   3. 正常：五行显示 YT/YN/YE/B/T（目标角/实际角/误差/基准速度/转向量）
 *
 * 软件 I2C 不自动清屏，OLED 非逐像素更新，残留字符需定期整屏清除防残影。
 */
static void oled_task(void *pvParameters)
{
    (void)pvParameters;
    bool attitude_seen = false;    /* 是否曾读到过有效姿态，区分"稳定中"与"失效" */
    uint16_t clear_count = 99;     /* 初始 99 确保首帧执行一次清屏 */

    OLED_Init();
    OLED_Clear();
    OLED_vsprint(0, 0, 16, "mpu init...");
    OLED_Refresh();

    for (;;) {
        app_attitude_t attitude = {0};
        app_yaw_status_t yaw_state = {0};

        if (app_tasks_get_attitude(&attitude)) {
            attitude_seen = true;
        }
        (void)app_tasks_get_yaw_status(&yaw_state);

        /* 软件 I2C 不会自动清除残留字符，定期整屏清除防残影。
         * 约每 100 帧 × 200ms ≈ 20s 清一次。 */
        clear_count++;
        if (clear_count >= 100) {
            OLED_Clear();
            clear_count = 0;
        }

        if (!attitude_seen) {
            /* 状态 1：等待 MPU 稳定（约 20s） */
            OLED_vsprint(0, 0, 16, "ICM stabilizing");
            OLED_vsprint(0, 16, 16, "keep IMU still  ");
            oled_print_yaw_target(&yaw_state);
            OLED_vsprint(0, 48, 16, "yaw not ready  ");
        } else if (!attitude.valid) {
            /* 状态 2：MPU 曾稳定过但现在失效（通信丢失等） */
            OLED_vsprint(0, 0, 16, "icm failure    ");
            OLED_vsprint(0, 16, 16, "yaw target: ---");
            OLED_vsprint(0, 32, 16, "yaw now   : ---");
            OLED_vsprint(0, 48, 16, "check ICM20602 ");
        } else {
            /* 状态 3：正常显示，5 行 */
            /* YT: 目标角 + ON/OFF */
            /* YN: 当前实际角 */
            /* YE: 误差角（目标 - 当前） */
            /* B: 基准速度 (RPM) */
            /* T: 转向量 (RPM) */
            int32_t tgt_abs = util_abs_i32(yaw_state.target_yaw_deg10);
            int32_t now_abs = util_abs_i32(attitude.yaw_deg10);
            int32_t err_abs = util_abs_i32(yaw_state.error_yaw_deg10);
            char tgt_sign = (yaw_state.target_yaw_deg10 < 0) ? '-' : ' ';
            char now_sign = (attitude.yaw_deg10 < 0) ? '-' : ' ';
            char err_sign = (yaw_state.error_yaw_deg10 < 0) ? '-' : ' ';

            OLED_vsprint(0, 0, 16, "YT:%c%3ld.%1ld %s", tgt_sign,
                         (long)(tgt_abs / 10), (long)(tgt_abs % 10),
                         yaw_state.enabled ? "ON " : "OFF");
            OLED_vsprint(0, 16, 16, "YN:%c%3ld.%1ld", now_sign,
                         (long)(now_abs / 10), (long)(now_abs % 10));
            OLED_vsprint(0, 32, 16, "YE:%c%3ld.%1ld", err_sign,
                         (long)(err_abs / 10), (long)(err_abs % 10));
            OLED_vsprint(0, 48, 16, "B:%4ld T:%4ld",
                         (long)yaw_state.base_speed_rpm,
                         (long)yaw_state.turn_rpm);
        }

        OLED_Refresh();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  任务创建与启动
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  初始化各 service（创建队列、PID、编码器），创建全部任务并启动调度器。
 * @note   成功后不返回；应在 GPIO/时钟/外设等硬件初始化完成后调用。
 *         如果任一 service init 失败，死循环停在此处，便于调试器捕捉。
 */
void app_tasks_start(void)
{
    /* 所有 service 必须全部初始化成功，否则不启动调度器 */
    if (!attitude_service_init() || !yaw_loop_service_init() ||
        !speed_service_init() || !stepper_service_init()) {
        while (1) {}
    }

    /* 创建全部 6 个 FreeRTOS 任务。
     * 优先级：数值越大优先级越高。
     * 栈大小（字）：按实际调用栈占用预留余量，值由压力测试确定。 */
    xTaskCreate(led_task,        "LED",      128, NULL, 1, NULL);
    xTaskCreate(attitude_task,   "MPU",      512, NULL, 2,
                &g_attitude_task_handle);
    xTaskCreate(yaw_loop_task,   "YAW_LOOP", 384, NULL, 4,
                &g_yaw_loop_task_handle);
    xTaskCreate(speed_loop_task, "SPD_LOOP", 512, NULL, 3,
                &g_speed_loop_task_handle);
    xTaskCreate(stepper_service_task, "STEPPER", 256, NULL, 2, NULL);
#if ACTUATOR_VALIDATION_ENABLED
    xTaskCreate(actuator_validation_task, "ACT_TEST", 256, NULL, 2, NULL);
#elif VISION_TRACKING_ENABLED
    xTaskCreate(vision_tracking_task, "VISION", 384, NULL, 3, NULL);
#else
    xTaskCreate(yaw_key_task,    "YAW_KEY",  192, NULL, 2, NULL);
#endif
#if BOARD_OLED_TASK_ENABLED
    xTaskCreate(oled_task, "OLED", 512, NULL, 1, NULL);
#endif

    vTaskStartScheduler();
    /* 调度器启动失败才会走到这里 */
    while (1) {}
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  ISR 分发：只做标志检查 + 任务通知，不在 ISR 中访问 I2C 等阻塞外设。
 *
 *  ISR 中的原则：
 *  1. 尽快返回，不做耗时操作；
 *  2. 不调用可能阻塞的 API；
 *  3. 使用 FromISR 后缀的 FreeRTOS API；
 *  4. 收集 xHigherPriorityTaskWoken，在 ISR 末尾统一做上下文切换。
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  GROUP1 复用中断处理程序。
 *         两个中断源共享同一个 GPIO GROUP1 中断线：
 *           - 右轮编码器 GPIO 双边沿中断
 *           - ICM20602 数据就绪 INT 引脚
 */
void GROUP1_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* 检查并处理右轮编码器中断 */
    if (encoder_right_int_is_pending()) {
        encoder_right_irq_handler();
    }

    /* 检查并处理 ICM20602 数据就绪中断 */
    if (icm20602_int_is_pending()) {
        icm20602_int_clear();
        if (g_attitude_task_handle != NULL) {
            vTaskNotifyGiveFromISR(g_attitude_task_handle,
                                   &xHigherPriorityTaskWoken);
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief  I2C0 控制器中断处理程序。
 *         ICM20602 异步读取完成后触发，通知 attitude_task 进行 Mahony 解算。
 *
 *         实际 I2C 缓冲区填充由 i2c0_irq_handler() 完成（设完成标志），
 *         本中断只负责任务通知，不读写传感器数据。
 */
void I2C_0_INST_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    i2c0_irq_handler();

    if (icm20602_async_is_complete() && g_attitude_task_handle != NULL) {
        vTaskNotifyGiveFromISR(g_attitude_task_handle,
                               &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief  TIMER_0 定时器中断处理程序（10ms 周期）。
 *         通知 yaw_loop_task 执行控制节拍。
 */
void TIMER_0_INST_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    switch (DL_TimerG_getPendingInterrupt(TIMER_0_INST)) {
    case DL_TIMER_IIDX_ZERO:
        /* 定时器计数归零（一个周期结束），通知 yaw 环任务 */
        if (g_yaw_loop_task_handle != NULL) {
            vTaskNotifyGiveFromISR(g_yaw_loop_task_handle,
                                   &xHigherPriorityTaskWoken);
        }
        break;
    default:
        break;
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief UART3 receive interrupt: forward MaixCAM bytes to the vision task.
 */
void UART_VISION_INST_IRQHandler(void)
{
    vision_uart_irq_handler();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  FreeRTOS 钩子与静态分配内存回调
 *
 *  当前使用动态内存分配（heap_4），但 configSUPPORT_STATIC_ALLOCATION=1
 *  时仍需提供 vApplicationGetIdleTaskMemory 和 vApplicationGetTimerTaskMemory
 *  回调函数，否则链接失败。
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  FreeRTOS 栈溢出钩子。
 *         发生栈溢出时死循环，便于调试器捕捉栈帧进行栈深度分析。
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    while (1) {}
}

/**
 * @brief  提供空闲任务的 TCB 与栈内存（静态分配配置要求）。
 *         即使使用动态分配也必须提供此回调。
 */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
    StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize)
{
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];

    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

/**
 * @brief  提供 FreeRTOS 定时器服务任务的 TCB 与栈内存（静态分配配置要求）。
 *         即使使用动态分配也必须提供此回调。
 */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
    StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize)
{
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
