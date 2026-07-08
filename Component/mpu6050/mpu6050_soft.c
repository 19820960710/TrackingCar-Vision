/**
 * @file    mpu6050_soft.c
 * @brief   MPU6050 软件姿态解算驱动 (Mahony 四元数互补滤波, 无 DMP)
 *
 * @note
 *   ── 移植来源 ──
 *   算法移植自 "MPU6050 例程 (HAL 库版本)" 的 MPU6050_Get_Angle_Plus()
 *   (作者: B站 小努班 UID:437280309, V1_5)。I2C 后端由 HAL_I2C_Mem_* 改为
 *   现有 mspm0_i2c_write/read, 延时由 HAL_Delay 改为 mspm0_delay_ms。
 *
 *   ── 硬件连接 ──
 *   I2C 总线: I2C0 (SysConfig 名 "I2C_MPU6050", 快速模式 400kHz), 硬件 I2C
 *   INT 引脚: 下降沿触发中断 (MPU6050 Data_Ready, 100Hz)
 *
 *   ── 解算原理 ──
 *   不使用 MPU6050 内部 DMP, 改为 MCU 端 Mahony 四元数互补滤波:
 *     1) 100Hz 采样率读取 6 轴原始数据 (突发 14 字节)
 *     2) 加速度计归一化 → 重力参考方向叉积求姿态误差
 *     3) PI 补偿陀螺仪 → 四元数积分 → 归一化
 *     4) 四元数 → pitch/roll/yaw (°)
 *   动态 Kp/Ki: 检测到剧烈运动 (|a|>1.2g) 时增大增益, 加快收敛。
 *   yaw 无磁力计绝对参考, 含静止锁定 + 手动漂移补偿 (0.00003f)。
 *
 *   ── 中断机制 ──
 *   MPU6050 INT_PIN_CFG=0x80 (低有效, 50us 脉冲) + INT_ENABLE=0x01 (Data_Ready)
 *   每个采样周期 (10ms) 拉低 INT → MSPM0 GROUP1 下降沿中断 → 通知 mpu_task
 *
 *   ── SysConfig 配置要求 ──
 *   I2C: 命名 "I2C_MPU6050", Controller Mode, Fast Mode (400kHz)
 *   GPIO: 命名 "GPIO_MPU6050", 引脚 "PIN_MPU6050_INT",
 *         输入/上拉/使能中断/Level 3 优先级/下降沿触发
 */

#include "ti_msp_dl_config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "mpu6050.h"
#include "mspm0_i2c.h"   /* MSPM0 硬件 I2C 底层驱动 */
#include "clock.h"       /* mspm0_delay_ms / mspm0_get_clock_ms */

/* ═══════════════════════════════════════════════════════════════════════════
 *  I2C 寄存器读写 (基于 mspm0_i2c 硬件 I2C)
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief MPU6050 7 位 I2C 从机地址 (WHO_AM_I=0x68) */
#define MPU6050_ADDR        0x68

static void MPU6050_Write_REG(uint8_t reg, uint8_t data)
{
    (void)mspm0_i2c_write(MPU6050_ADDR, reg, 1, &data);
}

static uint8_t MPU6050_Read_REG(uint8_t reg)
{
    uint8_t tmp = 0;
    if (mspm0_i2c_read(MPU6050_ADDR, reg, 1, &tmp) != 0)
        return 0;
    return tmp;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  寄存器地址定义
 * ═══════════════════════════════════════════════════════════════════════════ */
#define MPU6050_SMPLRT_DIV          0x19
#define MPU6050_CONFIG              0x1A
#define MPU6050_GYRO_CONFIG         0x1B
#define MPU6050_ACCEL_CONFIG        0x1C
#define MPU6050_FIFO_EN             0x23
#define MPU6050_INT_PIN_CFG         0x37  /* 中断引脚配置 */
#define MPU6050_INT_ENABLE          0x38  /* 中断使能 */
#define MPU6050_INT_STATUS          0x3A  /* 中断状态 (读清) */
#define MPU6050_ACCEL_XOUT_H        0x3B
#define MPU6050_ACCEL_XOUT_L        0x3C
#define MPU6050_ACCEL_YOUT_H        0x3D
#define MPU6050_ACCEL_YOUT_L        0x3E
#define MPU6050_ACCEL_ZOUT_H        0x3F
#define MPU6050_ACCEL_ZOUT_L        0x40
#define MPU6050_TEMP_OUT_H          0x41
#define MPU6050_TEMP_OUT_L          0x42
#define MPU6050_GYRO_XOUT_H         0x43
#define MPU6050_GYRO_XOUT_L         0x44
#define MPU6050_GYRO_YOUT_H         0x45
#define MPU6050_GYRO_YOUT_L         0x46
#define MPU6050_GYRO_ZOUT_H         0x47
#define MPU6050_GYRO_ZOUT_L         0x48
#define MPU6050_USER_CTRL           0x6A
#define MPU6050_PWR_MGMT_1          0x6B
#define MPU6050_PWR_MGMT_2          0x6C
#define MPU6050_WHO_AM_I            0x75

/* INT_PIN_CFG 位 */
#define BIT_ACTL                    0x80  /* 中断低有效 */
/* INT_ENABLE 位 */
#define BIT_DATA_RDY_EN             0x01  /* 数据就绪中断 */

/* 滤波器带宽 */
typedef enum {
    Band_256Hz = 0x00, Band_186Hz, Band_96Hz, Band_43Hz,
    Band_21Hz, Band_10Hz, Band_5Hz
} Filter_Typedef;

/* 陀螺仪量程 */
typedef enum {
    gyro_250  = 0x00, gyro_500  = 0x08,
    gyro_1000 = 0x10, gyro_2000 = 0x18
} GYRO_CONFIG_Typedef;

/* 加速度计量程 */
typedef enum {
    acc_2g  = 0x00, acc_4g  = 0x08,
    acc_8g  = 0x10, acc_16g = 0x18
} ACCEL_CONFIG_Typedef;

/* ═══════════════════════════════════════════════════════════════════════════
 *  全局数据 (供外部读取)
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief 欧拉角输出 (°), 由 MPU6050_Get_Attitude() 更新 */
float pitch, roll, yaw;

/** @brief 原始陀螺仪数据 (未校准) */
short gyro[3];

/** @brief 原始加速度计数据 */
short accel[3];

/* ═══════════════════════════════════════════════════════════════════════════
 *  模块状态
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief 初始化完成标志 (0=未就绪, 1=就绪) */
static int g_mpu6050_ready = 0;

/** @brief Z 轴陀螺仪零漂 (软校准 100 次均值) */
static float gyro_zero_z = 0.0f;

/* ═══════════════════════════════════════════════════════════════════════════
 *  Mahony 四元数解算内部状态 (静态, 跨调用保持)
 * ═══════════════════════════════════════════════════════════════════════════ */
static float q_w = 1.0f, q_x = 0.0f, q_y = 0.0f, q_z = 0.0f;
static float m_integralFBx = 0.0f, m_integralFBy = 0.0f, m_integralFBz = 0.0f;
static float m_locked_yaw = 0.0f;
static int8_t m_yaw_locked = 0;
static float m_last_gz = 0.0f;
static uint8_t m_stable_count = 0;

/** @brief 采样周期 (s). 100Hz → 0.01s. 算法中作积分步长 */
#define MAHONY_DT        0.01f

/** @brief 陀螺量程换算系数: ±250°/s → (250/32768)*(π/180) ≈ 0.000133 */
#define GYRO_SCALE_250   0.000133f

/** @brief 加速度归一化系数: ±2g → LSB/16384 */
#define ACC_SCALE_2G     (1.0f / 16384.0f)

/* ═══════════════════════════════════════════════════════════════════════════
 *  INT 引脚宏 (兼容不同 SysConfig 版本, 与原 DMP 版完全一致)
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
#error "MPU6050 INT SysConfig macros not found. Check GPIO_MPU6050_INT pin config."
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 *  寄存器初始化
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief MPU6050 寄存器配置 (无 DMP)
 *
 * 配置:
 *   - 复位 + 唤醒
 *   - 采样率 100Hz (SMPLRT_DIV=9, DLPF 已分频所以 1kHz/(1+9)=100Hz)
 *   - 滤波带宽 5Hz (CONFIG=Band_5Hz, 同时使能 DLPF 使采样分频生效)
 *   - 陀螺 ±250°/s, 加速度 ±2g
 *   - FIFO 关闭
 *   - INT 引脚: 低有效 50us 脉冲 (INT_PIN_CFG=0x80), 兼容下降沿
 *   - Data_Ready 中断使能 (INT_ENABLE=0x01), 每个采样周期拉低 INT
 *   - 时钟源: X 轴陀螺 PLL (PWR_MGMT_1=0x01)
 */
static void MPU6050_Register_init(void)
{
    uint8_t smplrt_div;

    MPU6050_Write_REG(MPU6050_PWR_MGMT_1, 0x80);  /* 复位 */
    mspm0_delay_ms(100);
    MPU6050_Write_REG(MPU6050_PWR_MGMT_1, 0x00);  /* 唤醒 */
    mspm0_delay_ms(10);

    /* 采样率: DLPF 使能后内部采样 1kHz, SMPLRT_DIV = 1kHz/rate - 1 */
    smplrt_div = (uint8_t)(1000 / 100 - 1);       /* 100Hz → 9 */
    MPU6050_Write_REG(MPU6050_SMPLRT_DIV, smplrt_div);

    MPU6050_Write_REG(MPU6050_CONFIG,        Band_5Hz);     /* DLPF 5Hz */
    MPU6050_Write_REG(MPU6050_GYRO_CONFIG,   gyro_250);     /* ±250°/s */
    MPU6050_Write_REG(MPU6050_ACCEL_CONFIG,  acc_2g);       /* ±2g */
    MPU6050_Write_REG(MPU6050_FIFO_EN,       0x00);         /* 关 FIFO */

    /* INT 引脚: 低有效, 推挽, 50us 脉冲 (不锁存) → 下降沿触发 */
    MPU6050_Write_REG(MPU6050_INT_PIN_CFG, BIT_ACTL);       /* 0x80 */
    MPU6050_Write_REG(MPU6050_INT_ENABLE,  BIT_DATA_RDY_EN);/* 0x01 */

    MPU6050_Write_REG(MPU6050_USER_CTRL,   0x00);           /* 关 FIFO/I2C主 */
    MPU6050_Write_REG(MPU6050_PWR_MGMT_1,   0x01);          /* X 轴陀螺 PLL */
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Z 轴软校准 (减小 yaw 零漂)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief 静止时采样 100 次 Z 轴陀螺, 取均值作为零点
 * @note  采样间隔 10ms (与采样周期一致), 总耗时约 1s
 */
static void MPU6050_SoftCalibrate_Z(void)
{
    uint16_t const calibration_samples = 100;
    float gz_sum = 0.0f;

    for (uint16_t i = 0; i < calibration_samples; i++) {
        int16_t gz = ((int16_t)MPU6050_Read_REG(MPU6050_GYRO_ZOUT_H) << 8)
                     | MPU6050_Read_REG(MPU6050_GYRO_ZOUT_L);
        gz_sum += (float)gz;
        mspm0_delay_ms(10);
    }
    gyro_zero_z = gz_sum / calibration_samples;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Mahony 四元数解算
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief 读取 6 轴原始数据 + Mahony 四元数解算 → 更新 pitch/roll/yaw
 *
 * 数据来源: 从 0x3B 突发读 14 字节 (AccX/Y/Z + 温度2B + GyroX/Y/Z)
 * 算法: Mahony 互补滤波 (动态 Kp/Ki, yaw 静止锁定, 手动漂移补偿)
 *
 * @note  全局变量更新: pitch/roll/yaw (°), gyro[3], accel[3]
 *        需在 INT 数据就绪后调用 (100Hz)
 */
static void MPU6050_Mahony_Update(void)
{
    uint8_t buf[14];
    int16_t acc_x, acc_y, acc_z;
    int16_t gyr_x, gyr_y, gyr_z;
    float ax, ay, az, gx, gy, gz;
    float recipNorm;
    float qDot1, qDot2, qDot3, qDot4;
    float twoKp, twoKi;

    /* ── 突发读 14 字节: 0x3B..0x48 (AccXYZ + Temp + GyroXYZ) ── */
    if (mspm0_i2c_read(MPU6050_ADDR, MPU6050_ACCEL_XOUT_H, 14, buf) != 0)
        return;

    acc_x = ((int16_t)buf[0]  << 8) | buf[1];
    acc_y = ((int16_t)buf[2]  << 8) | buf[3];
    acc_z = ((int16_t)buf[4]  << 8) | buf[5];
    /* buf[6..7] = 温度, 跳过 */
    gyr_x = ((int16_t)buf[8]  << 8) | buf[9];
    gyr_y = ((int16_t)buf[10] << 8) | buf[11];
    gyr_z = ((int16_t)buf[12] << 8) | buf[13];

    /* 暴露原始数据供外部使用 */
    accel[0] = acc_x; accel[1] = acc_y; accel[2] = acc_z;
    gyro[0]  = gyr_x; gyro[1]  = gyr_y; gyro[2]  = gyr_z;

    /* ── 单位换算 ──
     * 加速度: LSB / 16384 → g (±2g 量程)
     * 陀螺:   LSB × 0.000133 → rad/s (±250°/s, 已含 °→rad) */
    ax = (float)acc_x * ACC_SCALE_2G;
    ay = (float)acc_y * ACC_SCALE_2G;
    az = (float)acc_z * ACC_SCALE_2G;
    gx = (float)gyr_x * GYRO_SCALE_250;
    gy = (float)gyr_y * GYRO_SCALE_250;
    gz = (float)(gyr_z - (int16_t)gyro_zero_z) * GYRO_SCALE_250;

    /* ── 动态 Kp/Ki: 剧烈运动 (|a|>1.2g) 时增大增益 ── */
    {
        float absAcc = sqrtf(ax * ax + ay * ay + az * az);
        if (absAcc > 1.2f) {
            twoKp = 30.0f;
            twoKi = 0.1f;
        } else {
            twoKp = 20.0f;
            twoKi = 0.05f;
        }
    }

    /* ── yaw 静止锁定: Z 角速度连续稳定 → 锁定 yaw 防漂 ── */
    {
        const float GYRO_THRESHOLD = 0.002f;
        const uint8_t STABLE_SAMPLES = 20;

        if (fabsf(gz - m_last_gz) < GYRO_THRESHOLD) {
            m_stable_count++;
        } else {
            m_stable_count = 0;
            m_yaw_locked = 0;
        }
        m_last_gz = gz;

        if (m_stable_count >= STABLE_SAMPLES && m_yaw_locked == 0) {
            m_yaw_locked = 1;
            m_locked_yaw = atan2f(2.0f * (q_w * q_z + q_x * q_y),
                                  1.0f - 2.0f * (q_y * q_y + q_z * q_z))
                           * 57.29578f;
        }
    }

    /* ── 四元数微分 (陀螺积分) ── */
    qDot1 = 0.5f * (-q_x * gx - q_y * gy - q_z * gz);
    qDot2 = 0.5f * ( q_w * gx + q_y * gz - q_z * gy);
    qDot3 = 0.5f * ( q_w * gy - q_x * gz + q_z * gx);
    qDot4 = 0.5f * ( q_w * gz + q_x * gy - q_y * gx);

    /* ── 加速度归一化 ── */
    recipNorm = 1.0f / sqrtf(ax * ax + ay * ay + az * az);
    if (isnan(recipNorm) || isinf(recipNorm) || recipNorm == 0.0f) {
        recipNorm = 1.0f;
        ax = 0.0f; ay = 0.0f; az = 1.0f;
    }
    ax *= recipNorm;
    ay *= recipNorm;
    az *= recipNorm;

    /* ── 重力参考方向 (当前四元数预测) ── */
    {
        float q0q0 = q_w * q_w;
        float q0q1 = q_w * q_x;
        float q0q2 = q_w * q_y;
        float q1q3 = q_x * q_z;
        float q2q3 = q_y * q_z;
        float q3q3 = q_z * q_z;

        float halfvx = q1q3 - q0q2;
        float halfvy = q0q1 + q2q3;
        float halfvz = q0q0 - 0.5f + q3q3;

        /* 误差 = 测量方向 × 参考方向 */
        float halfex = (ay * halfvz - az * halfvy);
        float halfey = (az * halfvx - ax * halfvz);
        float halfez = (ax * halfvy - ay * halfvx);

        /* ── PI 补偿 ── */
        if (twoKi > 0.0f) {
            m_integralFBx += twoKi * halfex * MAHONY_DT;
            m_integralFBy += twoKi * halfey * MAHONY_DT;
            m_integralFBz += twoKi * halfez * MAHONY_DT;
            gx += m_integralFBx;
            gy += m_integralFBy;
            gz += m_integralFBz;
        } else {
            m_integralFBx = 0.0f;
            m_integralFBy = 0.0f;
            m_integralFBz = 0.0f;
        }

        /* ── 比例增益 ── */
        gx += twoKp * halfex;
        gy += twoKp * halfey;
        gz += twoKp * halfez;

        /* ── 四元数微分反馈修正 ── */
        qDot1 -= q_x * halfex + q_y * halfey + q_z * halfez;
        qDot2 += q_w * halfex - q_z * halfey + q_y * halfez;
        qDot3 += q_z * halfex + q_w * halfey - q_x * halfez;
        qDot4 += -q_y * halfex + q_x * halfey + q_w * halfez;
    }

    /* ── 四元数积分 ──
     * 末项 +0.00003f 为 yaw 零漂手动补偿 (板级相关, 观察阶段保留原始值) */
    q_w += qDot1 * MAHONY_DT;
    q_x += qDot2 * MAHONY_DT;
    q_y += qDot3 * MAHONY_DT;
    q_z += qDot4 * MAHONY_DT + 0.00003f;

    /* ── 归一化四元数 ── */
    recipNorm = 1.0f / sqrtf(q_w * q_w + q_x * q_x + q_y * q_y + q_z * q_z);
    if (isnan(recipNorm) || isinf(recipNorm)) {
        q_w = 1.0f; q_x = 0.0f; q_y = 0.0f; q_z = 0.0f;
        recipNorm = 1.0f;
    }
    q_w *= recipNorm;
    q_x *= recipNorm;
    q_y *= recipNorm;
    q_z *= recipNorm;

    /* ── 四元数 → 欧拉角 (Z-Y-X) ──
     * 观察阶段: 板级偏移置 0 (原算法 -2.3/+5 已移除) */
    roll  = atan2f(2.0f * (q_w * q_x + q_y * q_z),
                   1.0f - 2.0f * (q_x * q_x + q_y * q_y)) * 57.29578f;
    pitch = asinf(2.0f * (q_w * q_y - q_z * q_x)) * 57.29578f;

    if (m_yaw_locked) {
        yaw = m_locked_yaw;
    } else {
        yaw = atan2f(2.0f * (q_w * q_z + q_x * q_y),
                     1.0f - 2.0f * (q_y * q_y + q_z * q_z)) * 57.29578f;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  公开接口
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  初始化 MPU6050 (软件解算, 无 DMP)
 * @return 0=成功, 非0=失败 (I2C 通信异常 / 芯片未应答)
 *
 * @note   流程:
 *         1) I2C 总线初始化 + 死锁恢复
 *         2) 寄存器配置 (采样率 100Hz, 量程, 中断)
 *         3) Z 轴陀螺软校准 (100×10ms ≈ 1s)
 *         4) 标记就绪
 *
 *         初始化时间: ~1.2s (主要耗时在软校准采样)
 */
int MPU6050_Init(void)
{
    uint8_t whoami;

    g_mpu6050_ready = 0;

    /* ── I2C 总线初始化 + 死锁恢复 ── */
    mpu6050_i2c_init();
    if (!DL_GPIO_readPins(GPIOA, DL_GPIO_PIN_0)) {
        mpu6050_i2c_sda_unlock();
    }

    /* ── 寄存器配置 ── */
    MPU6050_Register_init();

    /* ── 校验通信 (WHO_AM_I 应为 0x68) ── */
    whoami = MPU6050_Read_REG(MPU6050_WHO_AM_I);
    if (whoami != MPU6050_ADDR) {
        /* 通信异常, 但仍标记就绪以观察原始数据 (调试用)
         * 生产环境应 return -1 */
        return -1;
    }

    /* ── Z 轴软校准 ── */
    MPU6050_SoftCalibrate_Z();

    /* ── 清 INT 状态残留 ── */
    (void)MPU6050_Read_REG(MPU6050_INT_STATUS);

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
 * @brief  读取 6 轴数据 + Mahony 解算 → 更新 pitch/roll/yaw
 * @return 0=成功, -2=MPU6050 未就绪
 * @note   在 INT 数据就绪通知后调用 (100Hz)
 *         更新全局变量: pitch, roll, yaw, gyro[3], accel[3]
 */
int MPU6050_Get_Attitude(void)
{
    if (!g_mpu6050_ready)
        return -2;

    MPU6050_Mahony_Update();
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  INT 引脚中断接口 (与原 DMP 版完全一致, 供 app_tasks ISR 使用)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  使能 MPU6050 INT 引脚中断 (下降沿)
 * @note   在 SYSCFG_DL_init() 之后调用
 *         优先级 3 (低于速度闭环定时器)
 */
void MPU6050_IntEnable(void)
{
    NVIC_ClearPendingIRQ(MPU6050_INT_IRQN);
    NVIC_SetPriority(MPU6050_INT_IRQN, 3);
    NVIC_EnableIRQ(MPU6050_INT_IRQN);
}

/**
 * @brief  检测 MPU6050 INT 中断是否待处理
 * @return 非0=有待处理中断
 */
int MPU6050_IntIsPending(void)
{
    return (DL_GPIO_getPendingInterrupt(MPU6050_INT_PORT) == MPU6050_INT_IIDX);
}

/**
 * @brief  清除 MPU6050 INT 中断标记
 */
void MPU6050_IntClear(void)
{
    DL_GPIO_clearInterruptStatus(MPU6050_INT_PORT, MPU6050_INT_PIN);
}
