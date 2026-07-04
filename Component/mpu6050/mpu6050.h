/**
 * @file    mpu6050.h
 * @brief   MPU6050 6 轴姿态传感器 DMP 驱动接口
 *
 * ── SysConfig 配置步骤 ──
 * I2C: 添加 I2C 模块, 命名 "I2C_MPU6050", 使能 Controller Mode,
 *      速度 Fast Mode (400kHz), 配置 SCL/SDA 引脚
 *
 * GPIO: 添加 GPIO 模块, 命名 "GPIO_MPU6050",
 *       引脚命名 "PIN_MPU6050_INT", 方向 Input,
 *       内部电阻 Pull-Up, 使能中断,
 *       优先级 Level 3 - Lowest, 触发方式 Falling Edge
 *
 * ── 使用说明 ──
 * @code
 *   MPU6050_IntEnable();       // 使能 INT 中断
 *   int ret = MPU6050_Init();  // 初始化传感器 + DMP
 *   if (ret == 0) {
 *       Read_Quad();           // 读取并转换为欧拉角
 *       // 使用全局变量: pitch, roll, yaw (单位: °)
 *   }
 * @endcode
 */

#ifndef _MPU6050_H_
#define _MPU6050_H_

/** @brief 原始陀螺仪数据 (DMP FIFO 输出) */
extern short gyro[3];

/** @brief 原始加速度计数据 (DMP FIFO 输出) */
extern short accel[3];

/** @brief 俯仰角 (°), Read_Quad() 后更新 */
extern float pitch;

/** @brief 横滚角 (°), Read_Quad() 后更新 */
extern float roll;

/** @brief 偏航角 (°), Read_Quad() 后更新 */
extern float yaw;

/**
 * @brief  初始化 MPU6050 + 加载 DMP 固件
 * @return 0=成功, 非0=失败（I2C 通信异常/芯片未应答/DMP 固件加载失败）
 * @note   耗时约 200ms (主要耗时在 DMP 固件加载)
 *         需先调用 MPU6050_IntEnable()
 */
int MPU6050_Init(void);

/**
 * @brief  查询 MPU6050 是否初始化完成
 * @return 1=就绪, 0=未就绪
 */
int MPU6050_IsReady(void);

/**
 * @brief  读取 DMP FIFO → 更新四元数 + 欧拉角
 * @return 0=成功, -1=FIFO 读取错误, -2=传感器未就绪
 * @note   更新全局变量: pitch, roll, yaw, gyro[3], accel[3], quat[4]
 *         需在中断上下文中调用 (被 GROUP1_IRQHandler 触发)
 */
int Read_Quad(void);

/**
 * @brief  使能 MPU6050 INT 引脚中断 (下降沿)
 * @note   应在 SYSCFG_DL_init() 之后调用
 */
void MPU6050_IntEnable(void);

/**
 * @brief  检测 MPU6050 INT 中断是否待处理
 * @return 非0=有待处理中断
 */
int MPU6050_IntIsPending(void);

/**
 * @brief  清除 MPU6050 INT 中断标记
 */
void MPU6050_IntClear(void);

#endif  /* #ifndef _MPU6050_H_ */
