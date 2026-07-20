
/**
 * @file    main.c
 * @brief   M0_Templant_FreeRTOS - MSPM0G3507 FreeRTOS 两轮自平衡小车工程模板
 * @note
 *   ── 硬件平台 ──
 *   主控: MSPM0G3507 @ 80MHz
 *   传感器: MPU6050 (6 轴 IMU, I2C0, DMP 姿态解算)
 *   显示: SSD1306 OLED 128×64 (软件 I2C, PA28/PA31)
 *   驱动: TB6612 双路直流电机驱动 (PWM 20kHz, TIMG0)
 *   编码器: 双路增量式 (物理右轮 QEI TIMG8, 左轮 GPIO 软件解码)
 *   yaw 步进电机: ZDT X42S (UART2 PA23/PA24, 115200 8N1, 地址 0x01)
 *   pitch 步进电机: ZDT X42S (UART1 PB6/PB7, 115200 8N1, 地址 0x01)
 *
 *   ── 软件架构 ──
 *   main.c: 硬件初始化 → 打印启动信息 → 启动 FreeRTOS 调度器
 *   Component/task/app_tasks.c: 全部 FreeRTOS 任务、队列、中断胶水层
 *     - LED 闪烁任务 (1Hz)
 *     - MPU6050 姿态采集任务 (50Hz, DMP 中断驱动)
 *     - yaw 角闭环任务 (10ms Timer 驱动，50ms 更新目标差速)
 *     - 速度闭环控制任务 (10ms 采样，30ms PI 更新)
 *     - yaw/pitch 两个 ZDT X42S 的命令队列与响应路由任务
 *     - 两阶段执行机构组合验证任务（由配置开关控制）
 *     - yaw 目标角切换任务 (PB21 每次 +45°)
 *     - OLED 显示任务 (200ms 刷新)
 *
 *   ── 控制算法 ──
 *   yaw 位置环输出左右轮差速目标；速度环使用 mm/s 位置式 PI，
 *   10ms 读取编码器，30ms 窗口计算速度并更新 PWM。
 */

#include "ti_msp_dl_config.h"   /* SysConfig 自动生成的硬件配置 */
#include "led/led.h"             /* LED 指示灯 (PA22) */
#include "led/key.h"             /* 用户按键 (PB21, 内部上拉, 按下低电平) */
#include "UART/stepper_uart.h"  /* yaw/pitch 两路独立 ZDT 字节流 */
#include "icm20602/icm20602.h"   /* ICM20602 6轴IMU (I2C0, Mahony软件解算) */
#include "tb6612/tb6612.h"       /* TB6612 双路电机驱动 (PWM + GPIO 方向) */
#include "encoder/encoder.h"     /* 双路增量编码器 (QEI + 软件解码) */
#include "task/app_tasks.h"      /* FreeRTOS 任务创建与启动 */

/* Keil read-only startup diagnostic: identifies initialization stalls. */
volatile uint32_t g_boot_stage = 0U;

/**
 * @brief  硬件初始化：依次初始化所有外设模块
 * @note   调用顺序有要求：
 *         1. SYSCFG_DL_init() 必须最先执行（SysConfig 生成的底层初始化）
 *         2. MPU6050 中断先使能，因为后续 I2C 初始化需要 INT 引脚就绪
 *         3. 外设初始化顺序不影响功能，但建议按依赖关系排列
 */
static void prvSetupHardware(void)
{
    g_boot_stage = 1U;
    /* ── 第 0 步：SysConfig 自动生成的基础硬件初始化 ──
     * 包括：时钟树配置 (80MHz)、所有 GPIO/I2C/UART/Timer 模块初始化 */
    SYSCFG_DL_init();
    g_boot_stage = 2U;

    /* ── 第 1 步：使能 ICM20602 INT 引脚中断 ──
     * 当前未接 INT 脚，姿态任务用 10ms 轮询; 保留中断框架供后续接线 */
    icm20602_int_enable();
    g_boot_stage = 3U;

    /* ── 第 2 步：初始化各外设模块 ── */
    led_init();       /* LED (PA22) 已由 SysConfig 初始化，此处留作扩展 */
    key_init();       /* 按键 (PB21) 初始化消抖状态 */
    g_boot_stage = 4U;
    if (!stepper_uart_init()) { /* UART2 yaw + UART1 pitch */
        g_boot_stage = 0xE001U;
        while (1) {}
    }
    g_boot_stage = 5U;
    tb6612_init();    /* 电机驱动: 设置方向脚为停止，启动 PWM 计数器 */
    encoder_init();   /* 编码器: 初始化计数/状态，使能右轮 GPIO 中断 */
    g_boot_stage = 6U;
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
    g_boot_stage = 7U;

    /* ── 创建所有 FreeRTOS 任务并启动调度器 ──
     * app_tasks_start() 内部调用 xTaskCreate() 创建应用任务，
     * 然后调用 vTaskStartScheduler() 启动调度器，不再返回 */
    app_tasks_start();

    /* ── 安全兜底: 理论上不会执行到此处 ── */
    while (1) {}
}
