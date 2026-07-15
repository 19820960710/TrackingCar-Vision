/**
 * @file    i2c_bus.h
 * @brief   MSPM0G3507 硬件 I2C0 底层驱动接口 (通用, 不绑定具体从机)
 *
 * @details 从原 Component/mpu6050/mspm0_i2c 解耦而来, 函数名去除 mpu6050 前缀,
 *          作为通用 I2C0 总线驱动供所有 I2C 从机模块复用 (ICM20602 等)。
 *          SysConfig 中 I2C 实例名仍为 "I2C_MPU6050" (历史命名, 不影响功能)。
 */
#ifndef _I2C_BUS_H_
#define _I2C_BUS_H_

#include <stdint.h>

/** @brief I2C0 总线初始化 (复位 + 配置引脚 + 上电)。 */
void i2c0_init(void);

/** @brief I2C0 SDA 总线死锁恢复 (GPIO bit-bang 时钟解锁)。 */
void i2c0_sda_unlock(void);

/**
 * @brief  I2C0 写操作
 * @param  slave_addr  7 位从机地址 (不含 R/W 位)
 * @param  reg_addr    寄存器地址
 * @param  length      数据长度 (≤ 255)
 * @param  data        数据数组
 * @return 0=成功, -1=超时/错误
 */
int i2c0_write(uint8_t slave_addr,
               uint8_t reg_addr,
               uint8_t length,
               uint8_t const *data);

/**
 * @brief  I2C0 读操作 (阻塞, 轮询)
 * @param  slave_addr  7 位从机地址 (不含 R/W 位)
 * @param  reg_addr    寄存器地址
 * @param  length      读取长度 (≤ 255)
 * @param  data        输出缓冲区
 * @return 0=成功, -1=超时/错误
 * @note   适合低频/初始化场景。高频读取(如 1kHz)请用 i2c0_read_async + 中断。
 */
int i2c0_read(uint8_t slave_addr,
              uint8_t reg_addr,
              uint8_t length,
              uint8_t *data);

/* ══════ 异步中断驱动读 (高频场景, 如 1kHz) ══════ */

/** @brief I2C0 异步读取状态 */
typedef enum {
    I2C0_ASYNC_IDLE = 0,     /**< 空闲 */
    I2C0_ASYNC_RX_STARTED,   /**< 已启动, 等待 RX_DONE 中断 */
    I2C0_ASYNC_RX_COMPLETE,  /**< 读取完成 (成功) */
    I2C0_ASYNC_ERROR         /**< 出错 (NACK/超时) */
} i2c0_async_status_t;

/**
 * @brief  启动一次异步 I2C0 读 (非阻塞, 立即返回)
 * @param  slave_addr  7 位从机地址
 * @param  reg_addr    寄存器地址
 * @param  length      读取长度 (≤ 255)
 * @param  data        输出缓冲区 (调用方持有, 完成后填充)
 * @return 0=已启动, -1=总线忙/参数错
 * @note   启动后 CPU 可做其他事; 完成后状态变 I2C0_ASYNC_RX_COMPLETE。
 *         需先调用 i2c0_enable_int() 使能 I2C 中断。需轮询 i2c0_async_get_status
 *         或在中断里用 vTaskNotifyGiveFromISR 通知任务。
 */
int i2c0_read_async(uint8_t slave_addr,
                    uint8_t reg_addr,
                    uint8_t length,
                    uint8_t *data);

/** 查询异步读状态。 */
i2c0_async_status_t i2c0_async_get_status(void);

/** 使能 I2C0 控制器中断 (异步读需要)。 */
void i2c0_enable_int(void);

/** I2C0 控制器中断处理 (供 I2C_0_INST_IRQHandler 调用)。 */
void i2c0_irq_handler(void);

#endif /* _I2C_BUS_H_ */
