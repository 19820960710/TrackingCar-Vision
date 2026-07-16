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
 * @brief  I2C0 读操作
 * @param  slave_addr  7 位从机地址 (不含 R/W 位)
 * @param  reg_addr    寄存器地址
 * @param  length      读取长度 (≤ 255)
 * @param  data        输出缓冲区
 * @return 0=成功, -1=超时/错误
 */
int i2c0_read(uint8_t slave_addr,
              uint8_t reg_addr,
              uint8_t length,
              uint8_t *data);

#endif /* _I2C_BUS_H_ */
