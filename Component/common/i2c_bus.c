/**
 * @file    i2c_bus.c
 * @brief   MSPM0G3507 硬件 I2C0 底层驱动 (通用)
 *
 * @details 从原 Component/mpu6050/mspm0_i2c 解耦, 逻辑不变, 仅改函数名:
 *            mpu6050_i2c_init      → i2c0_init
 *            mpu6050_i2c_sda_unlock→ i2c0_sda_unlock
 *            mspm0_i2c_write       → i2c0_write
 *            mspm0_i2c_read        → i2c0_read
 *          延时依赖从 clock.h 改为 delay.h。
 *
 *   ── I2C 实例 ──
 *   I2C_0_INST: SysConfig 中命名 "I2C_MPU6050" (历史命名), Controller Mode, 400kHz
 *
 *   ── 总线死锁恢复 ──
 *   从机异常复位时 SDA 可能被拉低 → 死锁。i2c0_sda_unlock() 产生 9 个 SCL
 *   时钟, SDA 恢复高电平即解锁。
 *
 *   ── 超时保护 ──
 *   所有等待循环均有超时 (I2C0_TIMEOUT_LOOPS = 500000, 约 500ms @ 80MHz)。
 */
#include "ti_msp_dl_config.h"
#include "delay.h"
#include "i2c_bus.h"

/* ═══════════════════════════════════════════════════════════════════════════
 *  配置常量
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief I2C 等待超时循环次数 (约相当于 500ms @ 80MHz) */
#define I2C0_TIMEOUT_LOOPS  (500000U)

/* ═══════════════════════════════════════════════════════════════════════════
 *  内部辅助
 * ═══════════════════════════════════════════════════════════════════════════ */

/* 等待 I2C 控制器进入空闲状态; 超时则尝试解锁 SDA 总线。 */
static int i2c0_wait_idle(void)
{
    uint32_t to = I2C0_TIMEOUT_LOOPS;

    while (!(DL_I2C_getControllerStatus(I2C_0_INST) &
             DL_I2C_CONTROLLER_STATUS_IDLE)) {
        if (--to == 0U) {
            i2c0_sda_unlock();
            return -1;
        }
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  公开接口: I2C 总线管理
 * ═══════════════════════════════════════════════════════════════════════════ */

void i2c0_init(void)
{
    DL_I2C_reset(I2C_0_INST);

    DL_GPIO_initPeripheralInputFunctionFeatures(GPIO_I2C_0_IOMUX_SDA,
        GPIO_I2C_0_IOMUX_SDA_FUNC, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initPeripheralInputFunctionFeatures(GPIO_I2C_0_IOMUX_SCL,
        GPIO_I2C_0_IOMUX_SCL_FUNC, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_enableHiZ(GPIO_I2C_0_IOMUX_SDA);
    DL_GPIO_enableHiZ(GPIO_I2C_0_IOMUX_SCL);

    DL_I2C_enablePower(I2C_0_INST);
    SYSCFG_DL_I2C_0_init();
}

void i2c0_sda_unlock(void)
{
    uint8_t cycleCnt = 0U;

    DL_I2C_reset(I2C_0_INST);

    DL_GPIO_initDigitalOutput(GPIO_I2C_0_IOMUX_SCL);

    DL_GPIO_initDigitalInputFeatures(GPIO_I2C_0_IOMUX_SDA,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_clearPins(GPIO_I2C_0_SCL_PORT, GPIO_I2C_0_SCL_PIN);
    DL_GPIO_enableOutput(GPIO_I2C_0_SCL_PORT, GPIO_I2C_0_SCL_PIN);

    do {
        DL_GPIO_clearPins(GPIO_I2C_0_SCL_PORT, GPIO_I2C_0_SCL_PIN);
        delay_ms(1);

        DL_GPIO_setPins(GPIO_I2C_0_SCL_PORT, GPIO_I2C_0_SCL_PIN);
        delay_ms(1);

        if (DL_GPIO_readPins(GPIO_I2C_0_SDA_PORT, GPIO_I2C_0_SDA_PIN)) {
            break;
        }
    } while (++cycleCnt < 100U);

    i2c0_init();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  公开接口: I2C 读写操作
 * ═══════════════════════════════════════════════════════════════════════════ */

int i2c0_write(uint8_t slave_addr,
               uint8_t reg_addr,
               uint8_t length,
               uint8_t const *data)
{
    uint8_t  txBuffer[256];
    uint16_t total;
    uint16_t sent;
    uint32_t to;

    if (length == 0U) {
        return 0;
    }

    txBuffer[0] = reg_addr;
    for (uint16_t i = 0; i < length; i++) {
        txBuffer[i + 1U] = data[i];
    }
    total = (uint16_t)length + 1U;

    if (i2c0_wait_idle() != 0) {
        return -1;
    }

    DL_I2C_flushControllerTXFIFO(I2C_0_INST);
    DL_I2C_clearInterruptStatus(I2C_0_INST,
        DL_I2C_INTERRUPT_CONTROLLER_TX_DONE);

    sent = DL_I2C_fillControllerTXFIFO(I2C_0_INST, txBuffer, total);

    DL_I2C_startControllerTransfer(I2C_0_INST, (uint32_t)slave_addr,
                                   DL_I2C_CONTROLLER_DIRECTION_TX, total);

    while (sent < total) {
        to = I2C0_TIMEOUT_LOOPS;
        while (!DL_I2C_getRawInterruptStatus(I2C_0_INST,
                                             DL_I2C_INTERRUPT_CONTROLLER_TXFIFO_EMPTY)) {
            if (--to == 0U) {
                i2c0_sda_unlock();
                return -1;
            }
        }
        sent += DL_I2C_fillControllerTXFIFO(I2C_0_INST,
                                             &txBuffer[sent], total - sent);
    }

    to = I2C0_TIMEOUT_LOOPS;
    while (!DL_I2C_getRawInterruptStatus(I2C_0_INST,
                                         DL_I2C_INTERRUPT_CONTROLLER_TX_DONE)) {
        if (--to == 0U) {
            i2c0_sda_unlock();
            return -1;
        }
    }

    DL_I2C_flushControllerTXFIFO(I2C_0_INST);
    return 0;
}

int i2c0_read(uint8_t slave_addr,
              uint8_t reg_addr,
              uint8_t length,
              uint8_t *data)
{
    uint16_t i = 0U;
    uint32_t to;

    if (length == 0U) {
        return 0;
    }

    if (i2c0_wait_idle() != 0) {
        return -1;
    }

    DL_I2C_flushControllerTXFIFO(I2C_0_INST);
    DL_I2C_flushControllerRXFIFO(I2C_0_INST);

    DL_I2C_transmitControllerData(I2C_0_INST, reg_addr);

    I2C_0_INST->MASTER.MCTR = I2C_MCTR_RD_ON_TXEMPTY_ENABLE;

    DL_I2C_clearInterruptStatus(I2C_0_INST,
        DL_I2C_INTERRUPT_CONTROLLER_RX_DONE);

    DL_I2C_startControllerTransfer(I2C_0_INST, (uint32_t)slave_addr,
                                   DL_I2C_CONTROLLER_DIRECTION_RX, length);

    while (!DL_I2C_getRawInterruptStatus(I2C_0_INST,
                                         DL_I2C_INTERRUPT_CONTROLLER_RX_DONE)) {
        if (!DL_I2C_isControllerRXFIFOEmpty(I2C_0_INST)) {
            if (i < length) {
                data[i++] = DL_I2C_receiveControllerData(I2C_0_INST);
            } else {
                (void)DL_I2C_receiveControllerData(I2C_0_INST);
            }
        }

        to = I2C0_TIMEOUT_LOOPS;
        while (DL_I2C_isControllerRXFIFOEmpty(I2C_0_INST) &&
               !DL_I2C_getRawInterruptStatus(I2C_0_INST,
                   DL_I2C_INTERRUPT_CONTROLLER_RX_DONE)) {
            if (--to == 0U) {
                I2C_0_INST->MASTER.MCTR = 0U;
                i2c0_sda_unlock();
                return -1;
            }
        }
    }

    while (!DL_I2C_isControllerRXFIFOEmpty(I2C_0_INST) && i < length) {
        data[i++] = DL_I2C_receiveControllerData(I2C_0_INST);
    }

    I2C_0_INST->MASTER.MCTR = 0U;

    DL_I2C_flushControllerTXFIFO(I2C_0_INST);
    DL_I2C_flushControllerRXFIFO(I2C_0_INST);

    return (i == length) ? 0 : -1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  异步中断驱动读 (高频场景, 如 1kHz IMU 读取)
 *
 *  机制: 启动读后立即返回, I2C 控制器在后台传输, 完成后触发 RX_DONE
 *  中断, i2c0_irq_handler 填充数据并置 I2C0_ASYNC_RX_COMPLETE。
 *  CPU 在传输期间可做其他事 (如姿态解算/控制), 不再阻塞轮询。
 *
 *  读时序沿用 i2c0_read 的 RD_ON_TXEMPTY 机制:
 *    START | AD+W | RA | (TX空自动 Repeated-Start) | AD+R | data... | STOP
 * ═══════════════════════════════════════════════════════════════════════════ */

static volatile i2c0_async_status_t g_async_status = I2C0_ASYNC_IDLE;
static volatile uint8_t *g_async_buf = NULL;      /* 接收缓冲区 */
static volatile uint16_t g_async_len = 0;          /* 期望读取字节数 */
static volatile uint16_t g_async_rx_count = 0;     /* 已接收字节数 */

int i2c0_read_async(uint8_t slave_addr,
                    uint8_t reg_addr,
                    uint8_t length,
                    uint8_t *data)
{
    if (length == 0U || data == NULL) {
        return -1;
    }
    if (g_async_status == I2C0_ASYNC_RX_STARTED) {
        return -1;   /* 上一次未完成, 拒绝重入 */
    }
    /* 异步读不用 i2c0_wait_idle (它超时会调 sda_unlock→i2c0_init 清 IMASK)。
     * 仅非阻塞检查空闲; 忙则返回 -1 让调用方下帧重试。 */
    if (!(DL_I2C_getControllerStatus(I2C_0_INST) & DL_I2C_CONTROLLER_STATUS_IDLE)) {
        return -1;
    }

    g_async_buf = data;
    g_async_len = length;
    g_async_rx_count = 0U;

    /* 若 IMASK 被意外清零 (运行中观测到), 重新使能。 */
    if (I2C_0_INST->CPU_INT.IMASK == 0U) {
        DL_I2C_enableInterrupt(I2C_0_INST,
            DL_I2C_INTERRUPT_CONTROLLER_RX_DONE |
            DL_I2C_INTERRUPT_CONTROLLER_RXFIFO_TRIGGER |
            DL_I2C_INTERRUPT_CONTROLLER_NACK |
            DL_I2C_INTERRUPT_CONTROLLER_ARBITRATION_LOST);
    }

    DL_I2C_flushControllerTXFIFO(I2C_0_INST);
    DL_I2C_flushControllerRXFIFO(I2C_0_INST);

    DL_I2C_transmitControllerData(I2C_0_INST, reg_addr);
    I2C_0_INST->MASTER.MCTR = I2C_MCTR_RD_ON_TXEMPTY_ENABLE;

    DL_I2C_clearInterruptStatus(I2C_0_INST,
        DL_I2C_INTERRUPT_CONTROLLER_RX_DONE |
        DL_I2C_INTERRUPT_CONTROLLER_NACK |
        DL_I2C_INTERRUPT_CONTROLLER_ARBITRATION_LOST);

    /* 中断事件已在 i2c0_enable_int() 中常使能, 此处不再开关。 */

    g_async_status = I2C0_ASYNC_RX_STARTED;

    DL_I2C_startControllerTransfer(I2C_0_INST, (uint32_t)slave_addr,
                                   DL_I2C_CONTROLLER_DIRECTION_RX, length);
    return 0;
}

i2c0_async_status_t i2c0_async_get_status(void)
{
    return g_async_status;
}

void i2c0_enable_int(void)
{
    NVIC_ClearPendingIRQ(I2C_0_INST_INT_IRQN);
    NVIC_SetPriority(I2C_0_INST_INT_IRQN, 3);
    NVIC_EnableIRQ(I2C_0_INST_INT_IRQN);

    /* 一次性使能 I2C 控制器事件中断。 */
    DL_I2C_clearInterruptStatus(I2C_0_INST,
        DL_I2C_INTERRUPT_CONTROLLER_RX_DONE |
        DL_I2C_INTERRUPT_CONTROLLER_RXFIFO_TRIGGER |
        DL_I2C_INTERRUPT_CONTROLLER_NACK |
        DL_I2C_INTERRUPT_CONTROLLER_ARBITRATION_LOST);
    DL_I2C_enableInterrupt(I2C_0_INST,
        DL_I2C_INTERRUPT_CONTROLLER_RX_DONE |
        DL_I2C_INTERRUPT_CONTROLLER_RXFIFO_TRIGGER |
        DL_I2C_INTERRUPT_CONTROLLER_NACK |
        DL_I2C_INTERRUPT_CONTROLLER_ARBITRATION_LOST);
}

void i2c0_irq_handler(void)
{
    /* 循环处理所有 pending 中断, 避免中断风暴 (IIDX 读取只清一个,
     * 若多个事件 pending 会反复进 ISR 耗尽 CPU)。 */
    DL_I2C_IIDX idx;
    while ((idx = DL_I2C_getPendingInterrupt(I2C_0_INST))
           != (DL_I2C_IIDX)0) {
        switch (idx) {
        case DL_I2C_IIDX_CONTROLLER_RX_DONE:
            /* 收尾: 读走 FIFO 剩余字节, 置完成 */
            while (!DL_I2C_isControllerRXFIFOEmpty(I2C_0_INST) &&
                   g_async_rx_count < g_async_len) {
                g_async_buf[g_async_rx_count++] = DL_I2C_receiveControllerData(I2C_0_INST);
            }
            I2C_0_INST->MASTER.MCTR = 0U;
            g_async_status = I2C0_ASYNC_RX_COMPLETE;
            break;

        case DL_I2C_IIDX_CONTROLLER_RXFIFO_TRIGGER:
            /* RX FIFO 达阈值 → 读走已到数据 */
            while (!DL_I2C_isControllerRXFIFOEmpty(I2C_0_INST)) {
                if (g_async_rx_count < g_async_len) {
                    g_async_buf[g_async_rx_count++] = DL_I2C_receiveControllerData(I2C_0_INST);
                } else {
                    (void)DL_I2C_receiveControllerData(I2C_0_INST);
                }
            }
            break;

        case DL_I2C_IIDX_CONTROLLER_NACK:
        case DL_I2C_IIDX_CONTROLLER_ARBITRATION_LOST:
            I2C_0_INST->MASTER.MCTR = 0U;
            g_async_status = I2C0_ASYNC_ERROR;
            break;

        case DL_I2C_IIDX_CONTROLLER_TX_DONE:
        case DL_I2C_IIDX_CONTROLLER_TXFIFO_TRIGGER:
            /* 异步读不主动发 TX, 但 init 阶段阻塞写可能残留; 忽略。 */
            break;

        default:
            break;
        }
    }
}
