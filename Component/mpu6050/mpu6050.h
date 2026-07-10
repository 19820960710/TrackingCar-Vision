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
 *   MPU6050_IntEnable();           // 使能 INT 中断
 *   int ret = MPU6050_Init();      // 初始化传感器 + DMP
 *   if (ret == 0) {
 *       mpu_attitude_t att;
 *       Read_Quad(&att);           // 读取并转换为欧拉角
 *       // att.pitch / att.roll / att.yaw (单位: °)
 *   }
 * @endcode
 */

#ifndef _MPU6050_H_
#define _MPU6050_H_

/**
 * @brief 欧拉角姿态输出 (单位: °)
 * @note  Read_Quad() 成功返回 0 时填充。Z-Y-X 旋转顺序。 */
typedef struct {
    float pitch;   /**< 俯仰角 (°) */
    float roll;    /**< 横滚角 (°) */
    float yaw;     /**< 偏航角 (°) */
} mpu_attitude_t;

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
 * @brief  读取 DMP FIFO → 四元数 → 欧拉角，写入 out
 * @param  out  姿态输出指针（不可为 NULL，成功时填充）
 * @return 0=成功, -1=FIFO 读取错误, -2=传感器未就绪
 * @note   需在中断上下文中调用 (被 GROUP1_IRQHandler 触发)
 */
int Read_Quad(mpu_attitude_t *out);

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
