/**
 * @file    icm20602.c
 * @brief   ICM20602 6 轴 IMU 驱动实现 (Mahony 四元数互补滤波, 无 DMP)
 *
 * @note
 *   ── 移植来源 ──
 *   基于本仓库历史 mpu6050_soft.c (fc5a0db) 的 Mahony 解算框架改造。
 *   ICM20602 寄存器布局与 MPU6050 高度兼容, 主要差异:
 *     - I2C 地址: 0x69 (MPU6050 为 0x68; SA0 接高)
 *     - WHO_AM_I: 0x12 (MPU6050 为 0x68)
 *     - 数据寄存器 0x3B..0x48 布局完全一致 (AccXYZ + Temp2B + GyroXYZ)
 *     - 量程寄存器位定义一致 (本驱动用 ±250°/s + ±2g)
 *   I2C 后端复用 mspm0_i2c_write/read (硬件 I2C0, PA0/PA1)。
 *
 *   ── 硬件连接 ──
 *   I2C: PA0(SDA)/PA1(SCL), I2C0 400kHz, 与原 MPU6050 共用
 *   INT: PB4, 当前未接线; 保留中断接口框架, 任务用 10ms 轮询
 *
 *   ── 解算原理 (Mahony 四元数互补滤波) ──
 *     1) 100Hz 采样率读取 6 轴原始数据 (突发 14 字节)
 *     2) 加速度计归一化 → 重力参考方向叉积求姿态误差
 *     3) PI 补偿陀螺仪 → 四元数积分 → 归一化
 *     4) 四元数 → pitch/roll/yaw (°)
 *   动态 Kp/Ki: 检测到剧烈运动 (|a|>1.2g) 时增大增益, 加快收敛。
 *   yaw 无磁力计绝对参考, 含静止锁定 + 运行时 Z 轴零偏追踪 (静止 3s
 *   自动重新校准零偏, 对抗温度漂移)。
 */

#include "ti_msp_dl_config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "icm20602/icm20602.h"
#include "common/i2c_bus.h"    /* 硬件 I2C0 底层驱动 */
#include "common/delay.h"      /* delay_ms / get_time_ms */

/* ═══════════════════════════════════════════════════════════════════════════
 *  I2C 寄存器读写 (基于 mspm0_i2c 硬件 I2C0)
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief ICM20602 7 位 I2C 从机地址 (SA0 接高 → 0x69) */
#define ICM20602_ADDR        0x69

static void icm_write_reg(uint8_t reg, uint8_t data)
{
    (void)i2c0_write(ICM20602_ADDR, reg, 1, &data);
}

static uint8_t icm_read_reg(uint8_t reg)
{
    uint8_t tmp = 0;
    if (i2c0_read(ICM20602_ADDR, reg, 1, &tmp) != 0)
        return 0;
    return tmp;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  寄存器地址定义 (与 MPU6050 兼容部分直接沿用; ICM20602 独有已注明)
 * ═══════════════════════════════════════════════════════════════════════════ */
#define ICM20602_SMPLRT_DIV          0x19
#define ICM20602_CONFIG              0x1A
#define ICM20602_GYRO_CONFIG         0x1B
#define ICM20602_ACCEL_CONFIG        0x1C
#define ICM20602_ACCEL_CONFIG_2      0x1D  /* ICM20602 独有: 加速度低通滤波 */
#define ICM20602_FIFO_EN             0x23
#define ICM20602_INT_PIN_CFG         0x37
#define ICM20602_INT_ENABLE          0x38
#define ICM20602_INT_STATUS          0x3A
#define ICM20602_ACCEL_XOUT_H        0x3B
#define ICM20602_ACCEL_XOUT_L        0x3C
#define ICM20602_ACCEL_YOUT_H        0x3D
#define ICM20602_ACCEL_YOUT_L        0x3E
#define ICM20602_ACCEL_ZOUT_H        0x3F
#define ICM20602_ACCEL_ZOUT_L        0x40
#define ICM20602_TEMP_OUT_H          0x41
#define ICM20602_TEMP_OUT_L          0x42
#define ICM20602_GYRO_XOUT_H         0x43
#define ICM20602_GYRO_XOUT_L         0x44
#define ICM20602_GYRO_YOUT_H         0x45
#define ICM20602_GYRO_YOUT_L         0x46
#define ICM20602_GYRO_ZOUT_H         0x47
#define ICM20602_GYRO_ZOUT_L         0x48
#define ICM20602_USER_CTRL           0x6A
#define ICM20602_PWR_MGMT_1          0x6B
#define ICM20602_PWR_MGMT_2          0x6C
#define ICM20602_WHO_AM_I            0x75

/** WHO_AM_I 期望值 (ICM20602 = 0x12) */
#define ICM20602_WHO_AM_I_VAL        0x12

/* INT_PIN_CFG 位 */
#define BIT_ACTL                    0x80  /* 中断低有效 */
/* INT_ENABLE 位 */
#define BIT_DATA_RDY_EN             0x01  /* 数据就绪中断 */

/* 滤波器带宽 (CONFIG 寄存器 DLPF_CFG) */
typedef enum {
    Band_250Hz = 0x00, Band_184Hz, Band_92Hz, Band_41Hz,
    Band_20Hz, Band_10Hz, Band_5Hz, Band_360Hz
} filter_t;

/* 陀螺仪量程 (GYRO_CONFIG bit[4:3]) */
typedef enum {
    gyro_250  = 0x00, gyro_500  = 0x08,
    gyro_1000 = 0x10, gyro_2000 = 0x18
} gyro_config_t;

/* 加速度计量程 (ACCEL_CONFIG bit[4:3]) */
typedef enum {
    acc_2g  = 0x00, acc_4g  = 0x08,
    acc_8g  = 0x10, acc_16g = 0x18
} accel_config_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  模块状态
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief 初始化完成标志 (0=未就绪, 1=就绪) */
static int g_icm20602_ready = 0;

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
static uint16_t m_stable_count = 0;   /* 静止稳定计数 (uint16_t, 封顶避免溢出) */

/** @brief 采样周期默认值 (s). 100Hz → 0.01s. 首次调用或时间戳异常时使用 */
#define MAHONY_DT_DEFAULT  0.01f

/** @brief 上次采样时间戳 (ms), 用于计算真实 dt */
static uint32_t m_last_time_ms = 0;

/** @brief 陀螺量程换算系数: ±250°/s → (250/32768)*(π/180) ≈ 0.000133 */
#define GYRO_SCALE_250   0.000133f

/** @brief 加速度归一化系数: ±2g → 1/16384 g/LSB */
#define ACC_SCALE_2G     (1.0f / 16384.0f)

/* ═══════════════════════════════════════════════════════════════════════════
 *  INT 引脚宏 (复用 SysConfig 中 GPIO_MPU6050_INT 配置, 引脚命名未改)
 * ═══════════════════════════════════════════════════════════════════════════ */
#if defined(GPIO_MPU6050_INT_PIN_MPU6050_INT_PIN)
#define ICM_INT_PORT    GPIO_MPU6050_INT_PORT
#define ICM_INT_PIN     GPIO_MPU6050_INT_PIN_MPU6050_INT_PIN
#define ICM_INT_IIDX    GPIO_MPU6050_INT_PIN_MPU6050_INT_IIDX
#define ICM_INT_IRQN    GPIO_MPU6050_INT_INT_IRQN
#elif defined(GPIO_MPU6050_INT_PIN)
#define ICM_INT_PORT    GPIO_MPU6050_INT_PORT
#define ICM_INT_PIN     GPIO_MPU6050_INT_PIN
#define ICM_INT_IIDX    GPIO_MPU6050_INT_IIDX
#define ICM_INT_IRQN    GPIO_MPU6050_INT_IRQN
#else
#error "MPU6050 INT SysConfig macros not found. Check main.syscfg GPIO_MPU6050_INT pin configuration."
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 *  寄存器初始化
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief ICM20602 寄存器配置
 *
 * 配置:
 *   - 复位 + 唤醒
 *   - 采样率 100Hz (SMPLRT_DIV=9, DLPF 使能后内部 1kHz/(1+9)=100Hz)
 *   - 陀螺低通 5Hz (CONFIG=Band_5Hz, 使能 DLPF 使采样分频生效)
 *   - 加速度低通 5Hz (ACCEL_CONFIG_2=Band_5Hz, ICM20602 独有)
 *   - 陀螺 ±250°/s, 加速度 ±2g
 *   - FIFO 关闭
 *   - INT 引脚: 低有效 50us 脉冲 (INT_PIN_CFG=0x80), 兼容下降沿
 *   - Data_Ready 中断使能 (INT_ENABLE=0x01); 当前未接 INT, 仅配置寄存器
 *   - 时钟源: X 轴陀螺 PLL (PWR_MGMT_1=0x01)
 */
static void icm_register_init(void)
{
    uint8_t smplrt_div;

    icm_write_reg(ICM20602_PWR_MGMT_1, 0x80);  /* 复位 */
    delay_ms(100);
    icm_write_reg(ICM20602_PWR_MGMT_1, 0x00);  /* 唤醒 */
    delay_ms(10);

    /* 采样率: DLPF 使能后内部采样 1kHz, SMPLRT_DIV = 1kHz/rate - 1 */
    smplrt_div = (uint8_t)(1000 / 1000 - 1);   /* 1kHz → 0 (传感器最大输出) */
    icm_write_reg(ICM20602_SMPLRT_DIV, smplrt_div);

    /* DLPF: 1kHz 下用 92Hz (Band_92Hz), 滤波延迟小, 适合高速解算。
     * 旧 100Hz 用 5Hz; 1kHz 若仍用 5Hz 会严重滞后。 */
    icm_write_reg(ICM20602_CONFIG,         Band_92Hz);   /* 陀螺 DLPF 92Hz */
    icm_write_reg(ICM20602_GYRO_CONFIG,    gyro_250);    /* ±250°/s */
    icm_write_reg(ICM20602_ACCEL_CONFIG,   acc_2g);      /* ±2g */
    icm_write_reg(ICM20602_ACCEL_CONFIG_2, Band_92Hz);   /* 加速度 DLPF 92Hz */
    icm_write_reg(ICM20602_FIFO_EN,        0x00);         /* 关 FIFO */

    /* INT 引脚: 低有效, 推挽, 50us 脉冲 (不锁存) → 下降沿触发。
     * 当前未接 INT 脚, 仅配置寄存器; 轮询模式下不依赖此中断。 */
    icm_write_reg(ICM20602_INT_PIN_CFG, BIT_ACTL);        /* 0x80 */
    icm_write_reg(ICM20602_INT_ENABLE,  BIT_DATA_RDY_EN); /* 0x01 */

    icm_write_reg(ICM20602_USER_CTRL,   0x00);            /* 关 FIFO/I2C主 */
    icm_write_reg(ICM20602_PWR_MGMT_1,   0x01);           /* X 轴陀螺 PLL */
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Z 轴软校准 (减小 yaw 零漂)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief 静止时采样 256 次 Z 轴陀螺, 取均值作为零点
 * @note  采样间隔 10ms, 总耗时约 2.5s。需保持模块静止。
 *        256 次 (原 100) 提高初始零偏精度, 减小残余漂移。
 *        运行时静止 3s 后还会自动追踪零偏 (见 icm_mahony_update)。
 */
static void icm_soft_calibrate_z(void)
{
    uint16_t const calibration_samples = 256;
    float gz_sum = 0.0f;

    for (uint16_t i = 0; i < calibration_samples; i++) {
        int16_t gz = ((int16_t)icm_read_reg(ICM20602_GYRO_ZOUT_H) << 8)
                     | icm_read_reg(ICM20602_GYRO_ZOUT_L);
        gz_sum += (float)gz;
        delay_ms(10);
    }
    gyro_zero_z = gz_sum / calibration_samples;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Mahony 四元数解算
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief 读取 6 轴原始数据 + Mahony 四元数解算 → 更新内部 pitch/roll/yaw
 *
 * 数据来源: 从 0x3B 突发读 14 字节 (AccX/Y/Z + 温度2B + GyroX/Y/Z)
 * 算法: Mahony 互补滤波 (动态 Kp/Ki, yaw 静止锁定, 手动漂移补偿)
 *
 * @param  buf  14 字节原始数据 (0x3B..0x48: AccXYZ + Temp + GyroXYZ)
 * @param  out  姿态输出 (°)
 * @return 0=成功
 */
static int icm_mahony_solve(const uint8_t buf[14], icm_attitude_t *out)
{
    int16_t acc_x, acc_y, acc_z;
    int16_t gyr_x, gyr_y, gyr_z;
    float ax, ay, az, gx, gy, gz;
    float recipNorm;
    float qDot1, qDot2, qDot3, qDot4;
    float twoKp, twoKi;
    float dt;
    uint32_t now_ms;

    acc_x = ((int16_t)buf[0]  << 8) | buf[1];
    acc_y = ((int16_t)buf[2]  << 8) | buf[3];
    acc_z = ((int16_t)buf[4]  << 8) | buf[5];
    /* buf[6..7] = 温度, 跳过 */
    gyr_x = ((int16_t)buf[8]  << 8) | buf[9];
    gyr_y = ((int16_t)buf[10] << 8) | buf[11];
    gyr_z = ((int16_t)buf[12] << 8) | buf[13];

    /* ── 计算真实 dt (s) ──
     * 用系统时间戳差值, 避免 vTaskDelay 周期不准导致积分比例失真
     * (旧版固定 0.01s, 实测旋转 90° 只到 ~80°, 偏小约 11%)。
     * 首次调用或时间戳回绕/异常时用默认值。 */
    (void)get_time_ms(&now_ms);
    if (m_last_time_ms == 0U) {
        dt = MAHONY_DT_DEFAULT;
    } else {
        uint32_t delta_ms = now_ms - m_last_time_ms;
        if (delta_ms == 0U || delta_ms > 100U) {
            /* 异常 (过短/溢出/首帧) → 用默认值, 避免发散 */
            dt = MAHONY_DT_DEFAULT;
        } else {
            dt = (float)delta_ms * 0.001f;
        }
    }
    m_last_time_ms = now_ms;

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

    /* ── yaw 静止锁定 + 运行时 Z 轴零偏追踪 ──
     * 纯陀螺 yaw 无磁力计绝对参考, Z 轴零漂必然累积。init 软校准只做一次,
     * 温度变化/长期运行后零偏会偏移。这里在静止判据下自动追踪零偏:
     *   - 200ms 连续稳定 → 锁定 yaw (抑制静止漂移)
     *   - 3s 连续稳定 → 一阶低通缓慢更新 gyro_zero_z (对抗温度漂移)
     * 判据严格 (gz 连续几乎不变), 避免吃掉真实缓慢转动。 */
    {
        const float GYRO_THRESHOLD = 0.002f;       /* gz 变化阈值 (rad/s) */
        const uint16_t STABLE_SAMPLES = 200;        /* 200ms 稳定 @1kHz → 锁定 yaw */
        const uint16_t CAL_STABLE_SAMPLES = 3000;   /* 3s 稳定 @1kHz → 追踪零偏 */
        const float ZERO_TRACK_ALPHA = 0.002f;      /* 一阶低通系数 (缓慢融合) */

        if (fabsf(gz - m_last_gz) < GYRO_THRESHOLD) {
            if (m_stable_count < 60000U) {          /* 封顶避免溢出 (uint16_t) */
                m_stable_count++;
            }
        } else {
            m_stable_count = 0;
            m_yaw_locked = 0;
        }
        m_last_gz = gz;

        /* 200ms 稳定 → 锁定 yaw 防静止漂移 */
        if (m_stable_count >= STABLE_SAMPLES && m_yaw_locked == 0) {
            m_yaw_locked = 1;
            m_locked_yaw = atan2f(2.0f * (q_w * q_z + q_x * q_y),
                                  1.0f - 2.0f * (q_y * q_y + q_z * q_z))
                           * 57.29578f;
        }

        /* 3s 连续稳定 → 运行时追踪 Z 轴零偏
         * 用原始 gyr_z (LSB) 更新 gyro_zero_z, 缓慢一阶低通融合。
         * 持续静止时每帧都微调, 对抗温度漂移; 一旦运动 m_stable_count 清零。 */
        if (m_stable_count >= CAL_STABLE_SAMPLES) {
            gyro_zero_z = gyro_zero_z * (1.0f - ZERO_TRACK_ALPHA)
                          + (float)gyr_z * ZERO_TRACK_ALPHA;
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

    /* ── 重力参考方向 (当前四元数预测) + 误差叉积 + PI 补偿 ── */
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

        float halfex = (ay * halfvz - az * halfvy);
        float halfey = (az * halfvx - ax * halfvz);
        float halfez = (ax * halfvy - ay * halfvx);

        if (twoKi > 0.0f) {
            m_integralFBx += twoKi * halfex * dt;
            m_integralFBy += twoKi * halfey * dt;
            m_integralFBz += twoKi * halfez * dt;
            gx += m_integralFBx;
            gy += m_integralFBy;
            gz += m_integralFBz;
        } else {
            m_integralFBx = 0.0f;
            m_integralFBy = 0.0f;
            m_integralFBz = 0.0f;
        }

        gx += twoKp * halfex;
        gy += twoKp * halfey;
        gz += twoKp * halfez;

        qDot1 -= q_x * halfex + q_y * halfey + q_z * halfez;
        qDot2 += q_w * halfex - q_z * halfey + q_y * halfez;
        qDot3 += q_z * halfex + q_w * halfey - q_x * halfez;
        qDot4 += -q_y * halfex + q_x * halfey + q_w * halfez;
    }

    /* ── 四元数积分 ──
     * Z 轴零漂由 icm_soft_calibrate_z (init 256 次均值) + 运行时静止追踪
     * (3s 稳定后一阶低通更新 gyro_zero_z) 双重抑制, 不再需要手动补偿值。 */
    q_w += qDot1 * dt;
    q_x += qDot2 * dt;
    q_y += qDot3 * dt;
    q_z += qDot4 * dt;

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

    /* ── 四元数 → 欧拉角 (Z-Y-X) ── */
    out->roll  = atan2f(2.0f * (q_w * q_x + q_y * q_z),
                        1.0f - 2.0f * (q_x * q_x + q_y * q_y)) * 57.29578f;
    out->pitch = asinf(2.0f * (q_w * q_y - q_z * q_x)) * 57.29578f;

    if (m_yaw_locked) {
        out->yaw = m_locked_yaw;
    } else {
        out->yaw = atan2f(2.0f * (q_w * q_z + q_x * q_y),
                          1.0f - 2.0f * (q_y * q_y + q_z * q_z)) * 57.29578f;
    }

    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  公开接口
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  初始化 ICM20602 (寄存器配置 + Z 轴陀螺软校准)
 * @return 0=成功, -1=I2C 通信异常, -2=WHO_AM_I 不匹配
 *
 * @note   流程:
 *         1) I2C 总线初始化 + 死锁恢复
 *         2) 寄存器配置 (采样率 100Hz, 量程 ±250°/s + ±2g, 中断)
 *         3) WHO_AM_I 校验 (期望 0x12)
 *         4) Z 轴陀螺软校准 (100×10ms ≈ 1s)
 *         5) 清 INT 状态残留, 标记就绪
 *
 *         初始化时间: ~1.2s (主要耗时在软校准采样)
 */
int icm20602_init(void)
{
    uint8_t whoami;

    g_icm20602_ready = 0;

    /* ── I2C 总线初始化 + 死锁恢复 ── */
    i2c0_init();
    if (!DL_GPIO_readPins(GPIOA, DL_GPIO_PIN_0)) {
        i2c0_sda_unlock();
    }

    /* ── 寄存器配置 ── */
    icm_register_init();

    /* ── 校验通信 (WHO_AM_I 应为 0x12) ── */
    whoami = icm_read_reg(ICM20602_WHO_AM_I);
    if (whoami != ICM20602_WHO_AM_I_VAL) {
        return -2;
    }

    /* ── Z 轴软校准 ── */
    icm_soft_calibrate_z();

    /* ── 清 INT 状态残留 ── */
    (void)icm_read_reg(ICM20602_INT_STATUS);

    g_icm20602_ready = 1;
    return 0;
}

int icm20602_is_ready(void)
{
    return g_icm20602_ready;
}

int icm20602_get_attitude(icm_attitude_t *out)
{
    uint8_t buf[14];

    if (out == NULL) {
        return -1;
    }
    if (!g_icm20602_ready) {
        return -2;
    }
    if (i2c0_read(ICM20602_ADDR, ICM20602_ACCEL_XOUT_H, 14, buf) != 0) {
        return -1;
    }
    return icm_mahony_solve(buf, out);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  异步读取 (1kHz 高频, I2C 中断驱动)
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint8_t g_async_buf[14];   /* 异步读接收缓冲区 (模块持有) */

int icm20602_async_start(void)
{
    if (!g_icm20602_ready) {
        return -1;
    }
    return i2c0_read_async(ICM20602_ADDR, ICM20602_ACCEL_XOUT_H, 14, g_async_buf);
}

bool icm20602_async_is_complete(void)
{
    return (i2c0_async_get_status() == I2C0_ASYNC_RX_COMPLETE);
}

int icm20602_async_finish(icm_attitude_t *out)
{
    if (out == NULL) {
        return -1;
    }
    if (i2c0_async_get_status() != I2C0_ASYNC_RX_COMPLETE) {
        return -1;   /* 未完成或出错 */
    }
    /* 复位异步状态为 IDLE, 允许下一次启动 */
    return icm_mahony_solve(g_async_buf, out);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  中断接口
 *  ── ICM20602 INT 引脚 (PB4): 保留框架, 当前未接线 ──
 *  ── I2C0 控制器中断: 异步读取完成通知, 1kHz 模式必需 ──
 * ═══════════════════════════════════════════════════════════════════════════ */

void icm20602_int_enable(void)
{
    /* ICM20602 INT 引脚 (PB4) 中断: 保留框架, 当前未接线 */
    NVIC_ClearPendingIRQ(ICM_INT_IRQN);
    NVIC_SetPriority(ICM_INT_IRQN, 3);
    NVIC_EnableIRQ(ICM_INT_IRQN);

    /* I2C0 控制器中断: 1kHz 异步读取必需 */
    i2c0_enable_int();
}

int icm20602_int_is_pending(void)
{
    return (DL_GPIO_getPendingInterrupt(ICM_INT_PORT) == ICM_INT_IIDX);
}

void icm20602_int_clear(void)
{
    DL_GPIO_clearInterruptStatus(ICM_INT_PORT, ICM_INT_PIN);
}
