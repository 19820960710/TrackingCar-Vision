/**
 * @file    mspm0_i2c.c
 * @brief   MSPM0G3507 硬件 I2C 底层驱动 (MPU6050 专用)
 * @note
 *   ── I2C 实例 ──
 *   I2C_0_INST: SysConfig 中命名为 "I2C_MPU6050", Controller Mode, Fast Mode (400kHz)
 *   专用于 MPU6050 通信, 不与其他设备共享总线
 *
 *   ── 总线死锁恢复 ──
 *   I2C 标准规定: 当 SCL 为高时, SDA 应由主机释放 (上拉电阻维持高电平).
 *   若从机异常复位或在传输过程中掉电, SDA 可能被拉低 → 总线死锁.
 *   mpu6050_i2c_sda_unlock(): 通过 GPIO bit-bang 产生 9 个 SCL 时钟,
 *   每次 SCL 上升沿检测 SDA 状态, SDA 恢复高电平即解锁成功.
 *
 *   ── 超时保护 ──
 *   所有 I2C 等待循环均有超时保护 (MPU6050_I2C_TIMEOUT_LOOPS = 500000),
 *   避免因硬件故障导致代码永久阻塞
 */

#include "ti_msp_dl_config.h"
#include "clock.h"
#include "mspm0_i2c.h"

/* ═══════════════════════════════════════════════════════════════════════════
 *  配置常量
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief I2C 等待超时循环次数 (约相当于 500ms @ 80MHz) */
#define MPU6050_I2C_TIMEOUT_LOOPS  (500000U)

/* ═══════════════════════════════════════════════════════════════════════════
 *  内部辅助
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  等待 I2C 控制器进入空闲状态
 * @return 0=空闲, -1=超时
 * @note   轮询 DL_I2C_CONTROLLER_STATUS_IDLE 标志位
 *         超时后自动执行 SDA 解锁, 避免死锁扩散
 */
static int mpu6050_i2c_wait_idle(void)
{
    uint32_t to = MPU6050_I2C_TIMEOUT_LOOPS;

    while (!(DL_I2C_getControllerStatus(I2C_0_INST) &
             DL_I2C_CONTROLLER_STATUS_IDLE)) {
        if (--to == 0U) {
            /* 超时 → 尝试解锁 SDA 总线 */
            mpu6050_i2c_sda_unlock();
            return -1;
        }
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  公开接口: I2C 总线管理
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  MSPM0 I2C 总线初始化
 * @note   流程:
 *         1) 复位 I2C 控制器
 *         2) 初始化 SDA/SCL 引脚功能 (外设输入模式)
 *         3) 使能 Hi-Z (开漏输出, 外部上拉电阻提供高电平)
 *         4) 使能 I2C 电源
 *         5) 调用 SYSCFG_DL_I2C_0_init() 完成最终配置
 *
 *         引脚模式说明:
 *           DL_GPIO_RESISTOR_NONE: 不使用内部上拉, 依赖外部上拉电阻
 *           DL_GPIO_HYSTERESIS_DISABLE: 不启用施密特触发 (I2C 总线电平稳定)
 *           Hi-Z: I2C 开漏输出 → 只能拉低不能推高, 高电平由外部上拉提供
 */
void mpu6050_i2c_init(void)
{
    /* 复位 I2C 控制器到初始状态 */
    DL_I2C_reset(I2C_0_INST);

    /* 配置 SDA 引脚: 外设输入模式 (I2C 控制器驱动) */
    DL_GPIO_initPeripheralInputFunctionFeatures(GPIO_I2C_0_IOMUX_SDA,
        GPIO_I2C_0_IOMUX_SDA_FUNC, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);

    /* 配置 SCL 引脚: 外设输入模式 */
    DL_GPIO_initPeripheralInputFunctionFeatures(GPIO_I2C_0_IOMUX_SCL,
        GPIO_I2C_0_IOMUX_SCL_FUNC, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);

    /* 使能开漏输出 (Hi-Z 模式) */
    DL_GPIO_enableHiZ(GPIO_I2C_0_IOMUX_SDA);
    DL_GPIO_enableHiZ(GPIO_I2C_0_IOMUX_SCL);

    /* 上电并初始化 I2C 控制器 */
    DL_I2C_enablePower(I2C_0_INST);
    SYSCFG_DL_I2C_0_init();
}

/**
 * @brief  I2C 总线 SDA 死锁解锁
 * @note   工作原理 (GPIO bit-bang 时钟恢复):
 *         1) 复位 I2C 控制器
 *         2) SCL → GPIO 输出模式 (主机模拟时钟)
 *         3) SDA → GPIO 输入模式 (检测从机释放)
 *         4) 产生 SCL 脉冲序列: 每次 SCL 上升沿后检查 SDA
 *         5) SDA 恢复高电平 → 解锁成功, 退出循环
 *         6) 最多尝试 100 次 (避免死循环)
 *         7) 重新初始化 I2C 为正常模式
 *
 *         时序:
 *           SCL: __|‾‾|__|‾‾|__  ...  (Bit-bang 时钟)
 *           SDA: __|_____|‾‾‾‾‾‾  ...  (从机释放后变高)
 *                   ↑ 解锁成功
 */
void mpu6050_i2c_sda_unlock(void)
{
    uint8_t cycleCnt = 0U;

    /* ── 临时切换到 GPIO 模式 ── */
    DL_I2C_reset(I2C_0_INST);

    /* SCL → 输出模式, 初始低电平 */
    DL_GPIO_initDigitalOutput(GPIO_I2C_0_IOMUX_SCL);

    /* SDA → 输入模式 (检测从机释放) */
    DL_GPIO_initDigitalInputFeatures(GPIO_I2C_0_IOMUX_SDA,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_clearPins(GPIO_I2C_0_SCL_PORT, GPIO_I2C_0_SCL_PIN);
    DL_GPIO_enableOutput(GPIO_I2C_0_SCL_PORT, GPIO_I2C_0_SCL_PIN);

    /* ── Bit-bang 时钟脉冲: 最多 9+8 = 17 个时钟 (一个完整 I2C 字节) ── */
    do {
        /* 产生一个时钟周期: 低 → 高 → 低 */
        DL_GPIO_clearPins(GPIO_I2C_0_SCL_PORT, GPIO_I2C_0_SCL_PIN);
        mspm0_delay_ms(1);  /* SCL 低电平 1ms (慢速, 确保从机识别) */

        DL_GPIO_setPins(GPIO_I2C_0_SCL_PORT, GPIO_I2C_0_SCL_PIN);
        mspm0_delay_ms(1);  /* SCL 高电平 1ms */

        /* 在 SCL 高电平时检查 SDA: 若释放, 解锁成功 */
        if (DL_GPIO_readPins(GPIO_I2C_0_SDA_PORT, GPIO_I2C_0_SDA_PIN)) {
            break;
        }
    } while (++cycleCnt < 100U);

    /* ── 恢复 I2C 正常模式 ── */
    mpu6050_i2c_init();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  公开接口: I2C 读写操作
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  I2C 写操作: slave_addr + reg_addr + data[]
 * @param  slave_addr  7 位从机地址
 * @param  reg_addr    寄存器起始地址
 * @param  length      数据字节数 (≤ 255)
 * @param  data        数据缓冲区
 * @return 0=成功, -1=超时或错误
 *
 * @note   协议: START | slave_addr(W) | reg_addr | data[0] | ... | data[n-1] | STOP
 *         发送缓冲区: txBuffer[0]=reg_addr, txBuffer[1..n]=data
 *         总量 = length + 1
 *         TX FIFO 填满后启动传输, 剩余数据在 TXFIFO_EMPTY 中断中补充
 */
int mspm0_i2c_write(unsigned char slave_addr,
                     unsigned char reg_addr,
                     unsigned char length,
                     unsigned char const *data)
{
    uint8_t  txBuffer[256];  /* 发送缓冲区 (1 reg_addr + 255 data max) */
    uint16_t total;           /* 总发送字节数 */
    uint16_t sent;            /* 已发送字节数 */
    uint32_t to;              /* 超时计数器 */

    if (length == 0U) {
        return 0;  /* 空操作, 成功 */
    }

    /* 构建发送缓冲区: [reg_addr][data[0]][data[1]]...[data[n-1]] */
    txBuffer[0] = reg_addr;
    for (uint16_t i = 0; i < length; i++) {
        txBuffer[i + 1U] = data[i];
    }
    total = (uint16_t)length + 1U;  /* 总长度 = 1 (reg_addr) + length (data) */

    /* 等待 I2C 总线空闲 */
    if (mpu6050_i2c_wait_idle() != 0) {
        return -1;
    }

    /* 准备传输: 清 TX FIFO + 清中断 + 填 FIFO */
    DL_I2C_flushControllerTXFIFO(I2C_0_INST);
    DL_I2C_clearInterruptStatus(I2C_0_INST,
        DL_I2C_INTERRUPT_CONTROLLER_TX_DONE);

    sent = DL_I2C_fillControllerTXFIFO(I2C_0_INST, txBuffer, total);

    /* 启动 I2C 写传输 (发送从机地址 + 方向位 W) */
    DL_I2C_startControllerTransfer(I2C_0_INST, (uint32_t)slave_addr,
                                   DL_I2C_CONTROLLER_DIRECTION_TX, total);

    /* 持续填充 TX FIFO 直到全部发送完成 */
    while (sent < total) {
        to = MPU6050_I2C_TIMEOUT_LOOPS;
        /* 等待 TX FIFO 变空 (需要补充数据) */
        while (!DL_I2C_getRawInterruptStatus(I2C_0_INST,
                                             DL_I2C_INTERRUPT_CONTROLLER_TXFIFO_EMPTY)) {
            if (--to == 0U) {
                mpu6050_i2c_sda_unlock();
                return -1;
            }
        }
        sent += DL_I2C_fillControllerTXFIFO(I2C_0_INST,
                                             &txBuffer[sent], total - sent);
    }

    /* 等待传输完成 (TX_DONE 中断标志) */
    to = MPU6050_I2C_TIMEOUT_LOOPS;
    while (!DL_I2C_getRawInterruptStatus(I2C_0_INST,
                                         DL_I2C_INTERRUPT_CONTROLLER_TX_DONE)) {
        if (--to == 0U) {
            mpu6050_i2c_sda_unlock();
            return -1;
        }
    }

    /* 清 TX FIFO 避免残留影响下次传输 */
    DL_I2C_flushControllerTXFIFO(I2C_0_INST);
    return 0;
}

/**
 * @brief  I2C 读操作: slave_addr + reg_addr → data[]
 * @param  slave_addr  7 位从机地址
 * @param  reg_addr    寄存器起始地址
 * @param  length      期望读取的字节数 (≤ 255)
 * @param  data        输出缓冲区
 * @return 0=成功, -1=超时或错误
 *
 * @note   协议: START | slave_addr(W) | reg_addr | RESTART | slave_addr(R) | data[0]..[n-1] | STOP
 *         MSPM0 用 "RD_ON_TXEMPTY" 机制: 发完 reg_addr 后自动切换为读模式
 *         等待 RX_DONE 中断标志, 从 RX FIFO 读取数据
 */
int mspm0_i2c_read(unsigned char slave_addr,
                    unsigned char reg_addr,
                    unsigned char length,
                    unsigned char *data)
{
    uint16_t i = 0U;
    uint32_t to;

    if (length == 0U) {
        return 0;  /* 空操作, 成功 */
    }

    /* 等待总线空闲 */
    if (mpu6050_i2c_wait_idle() != 0) {
        return -1;
    }

    /* 准备读传输 */
    DL_I2C_flushControllerTXFIFO(I2C_0_INST);
    DL_I2C_flushControllerRXFIFO(I2C_0_INST);

    /* 发送寄存器地址 */
    DL_I2C_transmitControllerData(I2C_0_INST, reg_addr);

    /* 设置 TX FIFO 空后自动切换到读模式 (Restart + 读方向) */
    I2C_0_INST->MASTER.MCTR = I2C_MCTR_RD_ON_TXEMPTY_ENABLE;

    DL_I2C_clearInterruptStatus(I2C_0_INST,
        DL_I2C_INTERRUPT_CONTROLLER_RX_DONE);

    /* 启动 I2C 传输: 先写 reg_addr, TX 空后自动切换读 */
    DL_I2C_startControllerTransfer(I2C_0_INST, (uint32_t)slave_addr,
                                   DL_I2C_CONTROLLER_DIRECTION_RX, length);

    /* 等待 RX_DONE 中断, 期间持续从 RX FIFO 读取数据 */
    while (!DL_I2C_getRawInterruptStatus(I2C_0_INST,
                                         DL_I2C_INTERRUPT_CONTROLLER_RX_DONE)) {
        /* RX FIFO 非空 → 读取字节 */
        if (!DL_I2C_isControllerRXFIFOEmpty(I2C_0_INST)) {
            if (i < length) {
                data[i++] = DL_I2C_receiveControllerData(I2C_0_INST);
            } else {
                /* 丢弃溢出字节 (保护缓冲区) */
                (void)DL_I2C_receiveControllerData(I2C_0_INST);
            }
        }

        /* 超时检查: 防止 RX FIFO 永久为空 */
        to = MPU6050_I2C_TIMEOUT_LOOPS;
        while (DL_I2C_isControllerRXFIFOEmpty(I2C_0_INST) &&
               !DL_I2C_getRawInterruptStatus(I2C_0_INST,
                   DL_I2C_INTERRUPT_CONTROLLER_RX_DONE)) {
            if (--to == 0U) {
                I2C_0_INST->MASTER.MCTR = 0U;  /* 取消自动切换 */
                mpu6050_i2c_sda_unlock();
                return -1;
            }
        }
    }

    /* 接收剩余的 FIFO 数据 (RX_DONE 后可能有残留) */
    while (!DL_I2C_isControllerRXFIFOEmpty(I2C_0_INST) && i < length) {
        data[i++] = DL_I2C_receiveControllerData(I2C_0_INST);
    }

    /* 恢复 MCTR 默认值 */
    I2C_0_INST->MASTER.MCTR = 0U;

    /* 清 FIFO */
    DL_I2C_flushControllerTXFIFO(I2C_0_INST);
    DL_I2C_flushControllerRXFIFO(I2C_0_INST);

    /* 检查是否读取了期望的字节数 */
    return (i == length) ? 0 : -1;
}
