/**
 * @file    main.c
 * @brief   M0_Templant_FreeRTOS - MSPM0G3507 FreeRTOS 两轮自平衡小车工程模板
 * @note
 *   ── 硬件平台 ──
 *   主控: MSPM0G3507 @ 80MHz
 *   传感器: MPU6050 (6 轴 IMU, I2C0, DMP 姿态解算)
 *   显示: SSD1306 OLED 128×64 (软件 I2C, PA28/PA31)
 *   驱动: TB6612 双路直流电机驱动 (PWM 20kHz, TIMG0)
 *   编码器: 双路增量式 (左硬件 QEI TIMG8, 右 GPIO 双边沿软件解码)
 *
 *   ── 软件架构 ──
 *   main.c: 硬件初始化 → 打印启动信息 → 启动 FreeRTOS 调度器
 *   Component/task/app_tasks.c: 全部 FreeRTOS 任务、队列、中断胶水层
 *     - LED 闪烁任务 (1Hz)
 *     - MPU6050 姿态采集任务 (50Hz, DMP 中断驱动)
 *     - yaw 角闭环任务 (10ms Timer 驱动，50ms 更新目标差速)
 *     - 速度闭环控制任务 (10ms 采样，50ms PID 更新)
 *     - yaw 目标角切换任务 (PB21 每次 +45°)
 *     - OLED 显示任务 (200ms 刷新)
 *
 *   ── 控制算法 ──
 *   yaw 位置环输出左右轮差速目标；速度环使用增量式 PID，
 *   10ms 读取编码器，50ms 窗口计算 RPM 并更新 PWM。
 */

#include "ti_msp_dl_config.h"   /* SysConfig 自动生成的硬件配置 */
#include "led/led.h"             /* LED 指示灯 (PA22) */
#include "led/key.h"             /* 用户按键 (PB21, 内部上拉, 按下低电平) */
#include "UART/uart0.h"          /* 调试串口 (printf 重定向 + 收发双任务) */
#include "mpu6050/mpu6050.h"     /* MPU6050 姿态传感器 (I2C0 + DMP) */
#include "tb6612/tb6612.h"       /* TB6612 双路电机驱动 (PWM + GPIO 方向) */
#include "encoder/encoder.h"     /* 双路增量编码器 (QEI + 软件解码) */
#include "task/app_tasks.h"      /* FreeRTOS 任务创建与启动 */

/**
 * @brief  硬件初始化：依次初始化所有外设模块
 * @note   调用顺序有要求：
 *         1. SYSCFG_DL_init() 必须最先执行（SysConfig 生成的底层初始化）
 *         2. MPU6050 中断先使能，因为后续 I2C 初始化需要 INT 引脚就绪
 *         3. 外设初始化顺序不影响功能，但建议按依赖关系排列
 */
static void prvSetupHardware(void)
{
    /* ── 第 0 步：SysConfig 自动生成的基础硬件初始化 ──
     * 包括：时钟树配置 (80MHz)、所有 GPIO/I2C/UART/Timer 模块初始化 */
    SYSCFG_DL_init();

    /* ── 第 1 步：使能 MPU6050 INT 引脚中断 ──
     * 使能下降沿中断，DMP 数据就绪时产生中断通知 mpu_task */
    MPU6050_IntEnable();

    /* ── 第 2 步：初始化各外设模块 ── */
    led_init();       /* LED (PA22) 已由 SysConfig 初始化，此处留作扩展 */
    key_init();       /* 按键 (PB21) 初始化消抖状态 */
    uart0_init();     /* 调试串口: 创建收发队列/互斥锁，使能 RX 中断 */
    tb6612_init();    /* 电机驱动: 设置方向脚为停止，启动 PWM 计数器 */
    encoder_init();   /* 编码器: 初始化计数/状态，使能右轮 GPIO 中断 */
}

/**
 * @brief  程序入口
 * @note   执行流程：
 *         1. 硬件初始化 (prvSetupHardware)
 *         2. 打印启动信息到串口
 *         3. 创建 FreeRTOS 任务并启动调度器 (app_tasks_start 不会返回)
 */
int main(void)
{
    /* ── 硬件初始化 ── */
    prvSetupHardware();

    /* ── 启动提示: 输出到 UART0 调试串口 (115200 8N1) ──
     * 此时调度器尚未启动，uart0_sendStr 直接阻塞发送 */
    uart0_sendStr("M0 Yaw+Speed Closed Loop Ready | yaw key | 80MHz\r\n");

    /* ── 创建所有 FreeRTOS 任务并启动调度器 ──
     * app_tasks_start() 内部调用 xTaskCreate() 创建应用任务，
     * 然后调用 vTaskStartScheduler() 启动调度器，不再返回 */
    app_tasks_start();

    /* ── 安全兜底: 理论上不会执行到此处 ── */
    while (1) {}
}
