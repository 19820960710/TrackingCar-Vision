/**
 * @file    icm20602.h
 * @brief   ICM20602 6 轴 IMU 驱动接口 (Mahony 软件姿态解算, 无 DMP)
 *
 * @note
 *   ── 硬件连接 ──
 *   I2C 总线: I2C0 (SysConfig 名 "I2C_MPU6050", 快速模式 400kHz)
 *             引脚 PA0(SDA)/PA1(SCL), 与原 MPU6050 共用硬件 I2C0
 *   INT 引脚: PB4 (SysConfig 名 "GPIO_MPU6050_INT"), 当前未接线
 *             保留 INT 接口框架, 读取任务用 10ms 定时轮询驱动
 *
 *   ── 芯片说明 ──
 *   ICM20602 是 InvenSense 6 轴 IMU (3 轴陀螺 + 3 轴加速度), 无内置 DMP。
 *   寄存器布局与 MPU6050 高度兼容, WHO_AM_I=0x12, 默认 I2C 地址 0x69。
 *   yaw 角需 MCU 端软件解算 (本驱动用 Mahony 四元数互补滤波)。
 *
 *   ── 使用说明 ──
 *   @code
 *   icm20602_int_enable();            // 使能 INT 引脚中断 (可选)
 *   if (icm20602_init() == 0) {       // 初始化 + 软校准
 *       mpu_attitude_t att;
 *       icm20602_get_attitude(&att);  // 读 6 轴 + Mahony 解算 → pitch/roll/yaw
 *   }
 *   @endcode
 */
#ifndef _ICM20602_H_
#define _ICM20602_H_

#include <stdint.h>

/**
 * @brief 欧拉角姿态输出 (单位: °)
 * @note  icm20602_get_attitude() 成功返回 0 时填充。Z-Y-X 旋转顺序。
 *        与原 mpu_attitude_t 字段一致, 便于 attitude_service 复用。 */
typedef struct {
    float pitch;   /**< 俯仰角 (°) */
    float roll;    /**< 横滚角 (°) */
    float yaw;     /**< 偏航角 (°) */
} icm_attitude_t;

/**
 * @brief  初始化 ICM20602 (寄存器配置 + Z 轴陀螺软校准)
 * @return 0=成功, 非0=失败 (I2C 通信异常 / WHO_AM_I 不匹配)
 * @note   耗时约 1.2s (主要耗时在 Z 轴软校准 100×10ms 采样)
 *         需先调用 SYSCFG_DL_init(); INT 使能可选 (轮询模式下不需要)
 */
int icm20602_init(void);

/**
 * @brief  查询 ICM20602 是否已就绪
 * @return 1=就绪, 0=未就绪
 */
int icm20602_is_ready(void);

/**
 * @brief  读取 6 轴原始数据 + Mahony 四元数解算 → pitch/roll/yaw
 * @param  out  姿态输出指针 (成功时填充, 单位: °)
 * @return 0=成功, -1=I2C 读取失败, -2=未就绪
 * @note   建议以固定周期 (10ms/100Hz) 调用。内部用 get_time_ms() 计算
 *         真实 dt 做积分, 避免轮询周期不准导致 yaw 比例失真。
 *         内部突发读 14 字节 (0x3B..0x48: AccXYZ + Temp + GyroXYZ)。
 */
int icm20602_get_attitude(icm_attitude_t *out);

/* ── INT 引脚接口 (保留框架, 当前未接线, 轮询模式下不依赖) ── */

/** 使能 INT 引脚中断 (下降沿)。在 SYSCFG_DL_init() 之后调用。 */
void icm20602_int_enable(void);

/** 检测 INT 中断是否待处理。非0=有待处理中断。 */
int icm20602_int_is_pending(void);

/** 清除 INT 中断标记。 */
void icm20602_int_clear(void);

#endif /* _ICM20602_H_ */
