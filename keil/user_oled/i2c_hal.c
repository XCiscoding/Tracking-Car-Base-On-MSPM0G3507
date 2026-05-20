#include "i2c_hal.h"

/**
 * 阻塞式 I2C 主机发送。
 * 原理：
 *   1. 等待总线空闲（避免前一次传输未结束就启动新的）
 *   2. 把数据填入 TX FIFO（oled.c 每次最多发 2 字节，FIFO 完全够用）
 *   3. 发起传输（DriverLib 自动加 Start / Stop）
 *   4. 忙等直到总线空闲
 */
void DL_I2C_Master_transmit(I2C_Regs *inst, uint8_t addr,
                             const uint8_t *buf, uint8_t len,
                             uint32_t timeout)
{
    uint32_t guard = timeout;

    while (!(DL_I2C_getControllerStatus(inst) &
             DL_I2C_CONTROLLER_STATUS_IDLE)) {
        if (guard-- == 0u) {
            return;
        }
    }

    DL_I2C_flushControllerTXFIFO(inst);

    DL_I2C_fillControllerTXFIFO(inst, (uint8_t *)buf, len);

    DL_I2C_startControllerTransfer(inst, addr,
        DL_I2C_CONTROLLER_DIRECTION_TX, len);

    guard = timeout;
    while (DL_I2C_getControllerStatus(inst) &
           DL_I2C_CONTROLLER_STATUS_BUSY_BUS) {
        if ((DL_I2C_getControllerStatus(inst) &
             DL_I2C_CONTROLLER_STATUS_ERROR) != 0u) {
            DL_I2C_resetControllerTransfer(inst);
            return;
        }
        if (guard-- == 0u) {
            DL_I2C_resetControllerTransfer(inst);
            return;
        }
    }
}
