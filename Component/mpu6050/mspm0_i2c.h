/**
 * @file    mspm0_i2c.h
 * @brief   MSPM0G3507 硬件 I2C 底层驱动接口 (MPU6050 专用)
 *
 * ── 总线管理 ──
 * - mpu6050_i2c_init(): 初始化 I2C 控制器 + 引脚
 * - mpu6050_i2c_sda_unlock(): 从死锁中恢复 SDA 总线
 *
 * ── 读写操作 ──
 * - mspm0_i2c_write(): 写寄存器 (slave_addr + reg_addr + data[])
 * - mspm0_i2c_read():  读寄存器 (slave_addr + reg_addr → data[])
 */
#ifndef _MSPM0_I2C_H_
#define _MSPM0_I2C_H_

/** @brief I2C 总线初始化 (复位 + 配置引脚 + 上电) */
void mpu6050_i2c_init(void);

/** @brief I2C SDA 总线死锁恢复 (GPIO bit-bang 时钟解锁) */
void mpu6050_i2c_sda_unlock(void);

/**
 * @brief  I2C 写操作
 * @param  slave_addr  7 位从机地址 (不含 R/W 位)
 * @param  reg_addr    寄存器地址
 * @param  length      数据长度 (≤ 255)
 * @param  data        数据数组
 * @return 0=成功, -1=超时/错误
 */
int mspm0_i2c_write(unsigned char slave_addr,
                     unsigned char reg_addr,
                     unsigned char length,
                     unsigned char const *data);

/**
 * @brief  I2C 读操作
 * @param  slave_addr  7 位从机地址 (不含 R/W 位)
 * @param  reg_addr    寄存器地址
 * @param  length      读取长度 (≤ 255)
 * @param  data        输出缓冲区
 * @return 0=成功, -1=超时/错误
 */
int mspm0_i2c_read(unsigned char slave_addr,
                    unsigned char reg_addr,
                    unsigned char length,
                    unsigned char *data);

#endif  /* #ifndef _MSPM0_I2C_H_ */
