#include "ti_msp_dl_config.h"
#include "clock.h"
#include "mspm0_i2c.h"

#define MPU6050_I2C_TIMEOUT_LOOPS  (500000U)

static int mpu6050_i2c_wait_idle(void)
{
    uint32_t to = MPU6050_I2C_TIMEOUT_LOOPS;

    while (!(DL_I2C_getControllerStatus(I2C_0_INST) & DL_I2C_CONTROLLER_STATUS_IDLE)) {
        if (--to == 0U) {
            mpu6050_i2c_sda_unlock();
            return -1;
        }
    }
    return 0;
}

void mpu6050_i2c_init(void)
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

void mpu6050_i2c_sda_unlock(void)
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
        mspm0_delay_ms(1);
        DL_GPIO_setPins(GPIO_I2C_0_SCL_PORT, GPIO_I2C_0_SCL_PIN);
        mspm0_delay_ms(1);
        if (DL_GPIO_readPins(GPIO_I2C_0_SDA_PORT, GPIO_I2C_0_SDA_PIN)) {
            break;
        }
    } while (++cycleCnt < 100U);

    mpu6050_i2c_init();
}

int mspm0_i2c_write(unsigned char slave_addr,
                     unsigned char reg_addr,
                     unsigned char length,
                     unsigned char const *data)
{
    uint8_t txBuffer[256];
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

    if (mpu6050_i2c_wait_idle() != 0) {
        return -1;
    }

    DL_I2C_flushControllerTXFIFO(I2C_0_INST);
    DL_I2C_clearInterruptStatus(I2C_0_INST, DL_I2C_INTERRUPT_CONTROLLER_TX_DONE);
    sent = DL_I2C_fillControllerTXFIFO(I2C_0_INST, txBuffer, total);
    DL_I2C_startControllerTransfer(I2C_0_INST, (uint32_t)slave_addr,
                                   DL_I2C_CONTROLLER_DIRECTION_TX, total);

    while (sent < total) {
        to = MPU6050_I2C_TIMEOUT_LOOPS;
        while (!DL_I2C_getRawInterruptStatus(I2C_0_INST,
                                             DL_I2C_INTERRUPT_CONTROLLER_TXFIFO_EMPTY)) {
            if (--to == 0U) {
                mpu6050_i2c_sda_unlock();
                return -1;
            }
        }
        sent += DL_I2C_fillControllerTXFIFO(I2C_0_INST, &txBuffer[sent], total - sent);
    }

    to = MPU6050_I2C_TIMEOUT_LOOPS;
    while (!DL_I2C_getRawInterruptStatus(I2C_0_INST, DL_I2C_INTERRUPT_CONTROLLER_TX_DONE)) {
        if (--to == 0U) {
            mpu6050_i2c_sda_unlock();
            return -1;
        }
    }

    DL_I2C_flushControllerTXFIFO(I2C_0_INST);
    return 0;
}

int mspm0_i2c_read(unsigned char slave_addr,
                    unsigned char reg_addr,
                    unsigned char length,
                    unsigned char *data)
{
    uint16_t i = 0U;
    uint32_t to;

    if (length == 0U) {
        return 0;
    }

    if (mpu6050_i2c_wait_idle() != 0) {
        return -1;
    }

    DL_I2C_flushControllerTXFIFO(I2C_0_INST);
    DL_I2C_flushControllerRXFIFO(I2C_0_INST);
    DL_I2C_transmitControllerData(I2C_0_INST, reg_addr);
    I2C_0_INST->MASTER.MCTR = I2C_MCTR_RD_ON_TXEMPTY_ENABLE;
    DL_I2C_clearInterruptStatus(I2C_0_INST, DL_I2C_INTERRUPT_CONTROLLER_RX_DONE);
    DL_I2C_startControllerTransfer(I2C_0_INST, (uint32_t)slave_addr,
                                   DL_I2C_CONTROLLER_DIRECTION_RX, length);

    while (!DL_I2C_getRawInterruptStatus(I2C_0_INST, DL_I2C_INTERRUPT_CONTROLLER_RX_DONE)) {
        if (!DL_I2C_isControllerRXFIFOEmpty(I2C_0_INST)) {
            if (i < length) {
                data[i++] = DL_I2C_receiveControllerData(I2C_0_INST);
            } else {
                (void)DL_I2C_receiveControllerData(I2C_0_INST);
            }
        }

        to = MPU6050_I2C_TIMEOUT_LOOPS;
        while (DL_I2C_isControllerRXFIFOEmpty(I2C_0_INST) &&
               !DL_I2C_getRawInterruptStatus(I2C_0_INST, DL_I2C_INTERRUPT_CONTROLLER_RX_DONE)) {
            if (--to == 0U) {
                I2C_0_INST->MASTER.MCTR = 0U;
                mpu6050_i2c_sda_unlock();
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
