/**
 * @file    mpu6050.c
 * @brief   MPU6050 6 轴姿态传感器 DMP 驱动 (InvenSense 官方库适配层)
 * @note
 *   ── 硬件连接 ──
 *   I2C 总线: I2C0 (SysConfig 名 "I2C_MPU6050", 快速模式 400kHz)
 *   INT 引脚: 下降沿触发中断 (DMP 数据就绪)
 *
 *   ── DMP (Digital Motion Processor) 说明 ──
 *   MPU6050 内部集成的协处理器，负责:
 *     1. 200Hz 高频采集陀螺仪+加速度计数据
 *     2. 6 轴传感器融合 (Mahony/Madgwick 算法)
 *     3. 四元数输出 (比 MCU 端解算精度更高)
 *     4. 手势检测 (Tap, Android Orientation)
 *
 *   DMP 输出速率: 50Hz (DEFAULT_MPU_HZ)
 *
 *   ── 坐标系 ──
 *   gyro_orientation 矩阵定义传感器安装方向, 当前为标准安装:
 *     X 轴反, Y 轴反, Z 轴正 (gyro_orientation = {-1,0,0, 0,-1,0, 0,0,1})
 *
 *   ── 欧拉角转换 ──
 *   四元数 (q30 格式, 2^30 = 1.0) → pitch/roll/yaw (单位: °)
 *   转换存储在全局变量 pitch/roll/yaw 中, 供外部读取
 *
 *   ── SysConfig 配置要求 ──
 *   I2C: 命名为 "I2C_MPU6050", 使能 Controller Mode, Fast Mode (400kHz)
 *   GPIO: 命名为 "GPIO_MPU6050", 引脚命名 "PIN_MPU6050_INT",
 *         输入/上拉/使能中断/Level 3 优先级/下降沿触发
 */

#include "ti_msp_dl_config.h"

#include "inv_mpu.h"                      /* InvenSense MPU 核心库 */
#include "inv_mpu_dmp_motion_driver.h"    /* InvenSense DMP 运动驱动 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "mpu6050.h"
#include "mspm0_i2c.h"  /* MSPM0 硬件 I2C 底层驱动 */

/* ═══════════════════════════════════════════════════════════════════════════
 *  模块状态
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief MPU6050 + DMP 初始化完成标志 (0=未就绪, 1=就绪) */
static int g_mpu6050_ready = 0;

/* ═══════════════════════════════════════════════════════════════════════════
 *  MPU6050 INT 引脚宏 (兼容不同 SysConfig 版本)
 * ═══════════════════════════════════════════════════════════════════════════ */
#if defined(GPIO_MPU6050_INT_PIN_MPU6050_INT_PIN)
#define MPU6050_INT_PORT    GPIO_MPU6050_INT_PORT
#define MPU6050_INT_PIN     GPIO_MPU6050_INT_PIN_MPU6050_INT_PIN
#define MPU6050_INT_IIDX    GPIO_MPU6050_INT_PIN_MPU6050_INT_IIDX
#define MPU6050_INT_IRQN    GPIO_MPU6050_INT_INT_IRQN
#elif defined(GPIO_MPU6050_INT_PIN)
#define MPU6050_INT_PORT    GPIO_MPU6050_INT_PORT
#define MPU6050_INT_PIN     GPIO_MPU6050_INT_PIN
#define MPU6050_INT_IIDX    GPIO_MPU6050_INT_IIDX
#define MPU6050_INT_IRQN    GPIO_MPU6050_INT_IRQN
#else
#error "MPU6050 INT SysConfig macros not found. Check main.syscfg GPIO_MPU6050_INT pin configuration."
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 *  DMP 功能标志
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief 客户端请求的数据类型 */
#define PRINT_ACCEL     (0x01)    /* 加速度计数据 */
#define PRINT_GYRO      (0x02)    /* 陀螺仪数据     */
#define PRINT_QUAT      (0x04)    /* 四元数          */

/** @brief 传感器使能标志 */
#define ACCEL_ON        (0x01)
#define GYRO_ON         (0x02)

/** @brief 运动状态 */
#define MOTION          (0)
#define NO_MOTION       (1)

/** @brief DMP 默认输出频率 (Hz) */
#define DEFAULT_MPU_HZ  (50)

/** @brief DMP 固件存储位置 (Flash) */
#define FLASH_SIZE      (512)
#define FLASH_MEM_START ((void*)0x1800)

/* ═══════════════════════════════════════════════════════════════════════════
 *  全局数据结构
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief UART 通信协议用接收结构体 */
struct rx_s {
    unsigned char header[3];
    unsigned char cmd;
};

/** @brief HAL (硬件抽象层) 状态 */
struct hal_s {
    unsigned char sensors;           /* 使能的传感器 (ACCEL_ON|GYRO_ON) */
    unsigned char dmp_on;            /* DMP 是否已启用 */
    unsigned char wait_for_tap;      /* 是否等待 Tap 手势 */
    volatile unsigned char new_gyro; /* 是否有新陀螺仪数据 */
    unsigned short report;           /* 报告类型 (PRINT_QUAT) */
    unsigned short dmp_features;     /* DMP 功能掩码 */
    unsigned char motion_int_mode;   /* 运动中断模式 */
    struct rx_s rx;                  /* UART 接收缓冲区 */
};
static struct hal_s hal = {0};

/* ── 全局传感器数据 (DMP 填充) ── */
unsigned long sensor_timestamp;  /* 传感器时间戳 */
short gyro[3], accel[3], sensors; /* 陀螺/加速度/传感器状态 */
unsigned char more;               /* FIFO 中是否还有数据 */
long quat[4];                     /* 四元数 (q30 格式: 2^30 = 1.0) */

/** @brief Q30 格式缩放因子: 2^30 = 1073741824 (四元数归一化系数) */
#define q30  (1073741824.0f)

/** @brief 全局欧拉角输出 (°), 由 Read_Quad() 更新 */
float pitch, roll, yaw;

/* ═══════════════════════════════════════════════════════════════════════════
 *  传感器方向矩阵
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  陀螺仪方向矩阵 (9 元素行主序)
 * @note   定义传感器芯片相对于电路板的安装方向
 *         当前为标准安装: X 反, Y 反, Z 正
 *         {-1, 0, 0}  表示: 芯片 X 轴 = 电路板 -X 轴
 *         { 0,-1, 0}  表示: 芯片 Y 轴 = 电路板 -Y 轴
 *         { 0, 0, 1}  表示: 芯片 Z 轴 = 电路板 +Z 轴
 */
static signed char gyro_orientation[9] = {-1, 0, 0,
                                           0,-1, 0,
                                           0, 0, 1};

/* ═══════════════════════════════════════════════════════════════════════════
 *  DMP 回调函数 (当前为空实现)
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Tap (轻敲) 检测回调 */
static void tap_cb(unsigned char direction, unsigned char count)
{
    /* 当前未启用 Tap 功能, 预留实现 */
}

/** @brief Android 屏幕方向检测回调 */
static void android_orient_cb(unsigned char orientation)
{
    /* 当前未启用方向检测, 预留实现 */
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  方向矩阵 → 标量转换 (InvenSense 官方实现)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  将方向矩阵的一行转换为 3 位编码
 * @param  row  方向矩阵的一行 (3 个 signed char)
 * @return 3 位编码 (bit0-2 = 第一个非零元素的编码)
 *
 * 编码表:
 *   +X=0, -X=4, +Y=1, -Y=5, +Z=2, -Z=6, error=7
 */
static inline unsigned short inv_row_2_scale(const signed char *row)
{
    unsigned short b;

    if (row[0] > 0)
        b = 0;       /* +X */
    else if (row[0] < 0)
        b = 4;       /* -X */
    else if (row[1] > 0)
        b = 1;       /* +Y */
    else if (row[1] < 0)
        b = 5;       /* -Y */
    else if (row[2] > 0)
        b = 2;       /* +Z */
    else if (row[2] < 0)
        b = 6;       /* -Z */
    else
        b = 7;       /* 错误 */

    return b;
}

/**
 * @brief  3×3 方向矩阵 → 9 位标量编码
 * @param  mtx  方向矩阵 (9 个 signed char)
 * @return 9 位标量 (每 3 位一个轴方向)
 *
 * 格式: [Row2(bit8-6)] [Row1(bit5-3)] [Row0(bit2-0)]
 * 例: 标准矩阵 {-1,0,0, 0,-1,0, 0,0,1} →
 *     Row0 = -X = 4, Row1 = -Y = 5, Row2 = +Z = 2
 *     标量 = (2 << 6) | (5 << 3) | 4 = 0b010_101_100 = 0xAC
 */
static inline unsigned short inv_orientation_matrix_to_scalar(
    const signed char *mtx)
{
    unsigned short scalar;

    /*
       6 种标准方向排列:
       XYZ  010_001_000 = Row0=+X,Row1=+Y,Row2=+Z
       XZY  001_010_000
       YXZ  010_000_001
       YZX  000_010_001
       ZXY  001_000_010
       ZYX  000_001_010
     */

    scalar  = inv_row_2_scale(mtx);           /* Row0 → bit0-2 */
    scalar |= inv_row_2_scale(mtx + 3) << 3;  /* Row1 → bit3-5 */
    scalar |= inv_row_2_scale(mtx + 6) << 6;  /* Row2 → bit6-8 */

    return scalar;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  公开接口
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  MPU6050 初始化 (包含 DMP 固件加载)
 * @return 0=成功, 非0=失败
 *
 * @note   初始化流程:
 *         1) MSPM0 I2C 总线初始化 (含死锁恢复)
 *         2) MPU 底层初始化 (mpu_init: 时钟源/加速度计/陀螺仪)
 *         3) 使能加速度计 + 陀螺仪 → FIFO
 *         4) 设置采样率 = DEFAULT_MPU_HZ (50Hz)
 *         5) 回读并验证配置
 *         6) 加载 DMP 固件 (inv_mpu_dmp_motion_driver.c 中的固件镜像)
 *         7) 设置传感器方向矩阵
 *         8) 注册手势回调 (Tap, Android Orientation)
 *         9) 启用 DMP 功能 (6 轴四元数 + 原始加速度 + 校准陀螺仪)
 *         10) 设置 FIFO 输出速率
 *         11) 启动 DMP
 *
 *         典型初始化时间: ~200ms (主要耗时在 DMP 固件加载)
 */
int MPU6050_Init(void)
{
    int result;
    unsigned char accel_fsr;
    unsigned short gyro_rate, gyro_fsr;

    g_mpu6050_ready = 0;

    /* ── Step 1: I2C 总线初始化 + 死锁恢复 ── */
    mpu6050_i2c_init();

    /* 检查 I2C SDA 是否被从机拉低 (死锁检测): 若 SDA 为低, 执行解锁 */
    if (!DL_GPIO_readPins(GPIOA, DL_GPIO_PIN_0)) {
        mpu6050_i2c_sda_unlock();
    }

    /* ── Step 2: MPU 底层初始化 ──
     * mpu_init(): 复位 MPU6050, 配置时钟源 (陀螺仪 PLL), 设置默认量程 */
    result = mpu_init();
    if (result)
        return result;

    result = 0;

    /* ── Step 3-4: 使能传感器 + 配置 FIFO + 设置采样率 ── */
    result += mpu_set_sensors(INV_XYZ_GYRO | INV_XYZ_ACCEL);  /* 使能所有轴 */
    result += mpu_configure_fifo(INV_XYZ_GYRO | INV_XYZ_ACCEL); /* 陀螺+加计入 FIFO */
    result += mpu_set_sample_rate(DEFAULT_MPU_HZ);              /* 50Hz 采样率 */

    /* ── Step 5: 回读验证配置 ── */
    result += mpu_get_sample_rate(&gyro_rate);
    result += mpu_get_gyro_fsr(&gyro_fsr);
    result += mpu_get_accel_fsr(&accel_fsr);

    /* ── Step 5.5: 初始化 HAL 状态 ── */
    memset(&hal, 0, sizeof(hal));
    hal.sensors  = ACCEL_ON | GYRO_ON;  /* 陀螺 + 加计 */
    hal.report   = PRINT_QUAT;          /* 请求四元数输出 */

    /* ── Step 6-11: DMP 设置 ──
     * DMP 固件来自 inv_mpu_dmp_motion_driver.h, 被烧录到 MPU6050 内部 RAM
     * DMP_FEATURE_6X_LP_QUAT: 6 轴低功耗四元数 (200Hz 内部推估)
     * DMP_FEATURE_TAP: 轻敲检测 (未使用)
     * DMP_FEATURE_ANDROID_ORIENT: 屏幕方向检测 (未使用)
     * DMP_FEATURE_SEND_RAW_ACCEL: 原始加速度数据入 FIFO
     * DMP_FEATURE_SEND_CAL_GYRO: 校准后的陀螺数据入 FIFO
     * DMP_FEATURE_GYRO_CAL: 8 秒静止后自动校准陀螺零偏 */
    result += dmp_load_motion_driver_firmware();
    result += dmp_set_orientation(
        inv_orientation_matrix_to_scalar(gyro_orientation));
    result += dmp_register_tap_cb(tap_cb);
    result += dmp_register_android_orient_cb(android_orient_cb);

    hal.dmp_features = DMP_FEATURE_6X_LP_QUAT | DMP_FEATURE_TAP |
        DMP_FEATURE_ANDROID_ORIENT | DMP_FEATURE_SEND_RAW_ACCEL |
        DMP_FEATURE_SEND_CAL_GYRO | DMP_FEATURE_GYRO_CAL;

    result += dmp_enable_feature(hal.dmp_features);
    result += dmp_set_fifo_rate(DEFAULT_MPU_HZ);  /* FIFO 输出速率 = 50Hz */
    result += mpu_set_dmp_state(1);               /* 启动 DMP */

    hal.dmp_on = 1;

    if (result)
        return result;

    /* 标记初始化完成 */
    g_mpu6050_ready = 1;
    return 0;
}

/**
 * @brief  查询 MPU6050 是否已就绪
 * @return 1=就绪, 0=未就绪
 */
int MPU6050_IsReady(void)
{
    return g_mpu6050_ready;
}

/**
 * @brief  使能 MPU6050 INT 引脚中断 (下降沿)
 * @note   在 GROUP1_IRQHandler 中检测并通知 mpu_task
 *         优先级 3, 低于速度闭环定时器 (保证控制周期)
 *         应在 SYSCFG_DL_init() 之后调用
 */
void MPU6050_IntEnable(void)
{
    NVIC_ClearPendingIRQ(MPU6050_INT_IRQN);
    NVIC_SetPriority(MPU6050_INT_IRQN, 3);
    NVIC_EnableIRQ(MPU6050_INT_IRQN);
}

/**
 * @brief  检测 MPU6050 INT 中断是否待处理
 * @return 非 0 = 有待处理中断, 0 = 无
 * @note   用于 GROUP1_IRQHandler 中断分发
 */
int MPU6050_IntIsPending(void)
{
    return (DL_GPIO_getPendingInterrupt(MPU6050_INT_PORT) == MPU6050_INT_IIDX);
}

/**
 * @brief  清除 MPU6050 INT 中断标记
 * @note   在 GROUP1_IRQHandler 中读取数据前调用
 */
void MPU6050_IntClear(void)
{
    DL_GPIO_clearInterruptStatus(MPU6050_INT_PORT, MPU6050_INT_PIN);
}

/**
 * @brief  读取 DMP FIFO 数据并转换为欧拉角
 * @return 0=成功, -1=FIFO 读取错误, -2=MPU6050 未就绪
 *
 * @note   DMP FIFO 工作机制:
 *         1) DMP 在 200Hz 内部循环中推估四元数
 *         2) 按 dmp_set_fifo_rate(50) 的速率将四元数放入 FIFO
 *         3) FIFO 满或数据量达到阈值时, MPU6050 拉低 INT 引脚
 *         4) MCU 响应中断, 调用 dmp_read_fifo() 读取数据
 *         5) 四元数转换为欧拉角 (pitch/roll/yaw), 单位: °
 *
 *         四元数 → 欧拉角公式 (Z-Y-X 顺序):
 *           pitch = arcsin(-2*q1*q3 + 2*q0*q2) * 57.3
 *           roll  = atan2(2*q2*q3 + 2*q0*q1, -2*q1² - 2*q2² + 1) * 57.3
 *           yaw   = atan2(2*(q1*q2 + q0*q3), q0² + q1² - q2² - q3²) * 57.3
 *         57.3 = 180/π (弧度 → 度)
 *
 *         全局变量更新:
 *           pitch, roll, yaw: 欧拉角 (°)
 *           gyro[3], accel[3]: 原始传感器数据
 *           quat[4]: 四元数 (q30 格式)
 */
int Read_Quad(void)
{
    int result;

    if (!g_mpu6050_ready)
        return -2;  /* MPU6050 未初始化, 直接返回 */

    /* ── 循环读取 FIFO 直到为空 ──
     * DMP 可能一次存入多包数据 (例如中断响应延迟),
     * 循环读取确保拿到最新的数据, 丢掉旧数据 */
    do {
        result = dmp_read_fifo(gyro, accel, quat, &sensor_timestamp,
                               &sensors, &more);
    } while (more);

    if (result)
        return -1;  /* FIFO 读取错误 */

    /* ── 四元数 (Q30) → 浮点归一化 ──
     * Q30: 2^30 = 1073741824 表示 1.0 */
    float q0 = quat[0] / q30;  /* 实部 (w) */
    float q1 = quat[1] / q30;  /* 虚部 x */
    float q2 = quat[2] / q30;  /* 虚部 y */
    float q3 = quat[3] / q30;  /* 虚部 z */

    /* ── 四元数 → 欧拉角 (Z-Y-X 旋转顺序) ──
     * 单位四元数: q0² + q1² + q2² + q3² = 1
     * 转换公式来源于旋转矩阵的反正切提取 */
    pitch = asin(-2 * q1 * q3 + 2 * q0 * q2) * 57.3;  /* 俯仰角 */
    roll  = atan2(2 * q2 * q3 + 2 * q0 * q1,
                  -2 * q1 * q1 - 2 * q2 * q2 + 1) * 57.3; /* 横滚角 */
    yaw   = atan2(2 * (q1 * q2 + q0 * q3),
                  q0 * q0 + q1 * q1 - q2 * q2 - q3 * q3) * 57.3; /* 偏航角 */

    return 0;
}
