#include "i2c_hal.h"

/**
 * 阻塞式 I2C 主机发送，支持任意长度数据包。
 * 对于 len > FIFO深度（通常 8）的大包，在传输过程中循环填充 FIFO。
 */
void DL_I2C_Master_transmit(I2C_Regs *inst, uint8_t addr,
                             const uint8_t *buf, uint8_t len,
                             uint32_t timeout)
{
    uint32_t guard = timeout;
    const uint8_t *p = buf;
    uint16_t loaded;
    uint8_t  remaining;

    /* 第 1 步：等待总线空闲 */
    while (!(DL_I2C_getControllerStatus(inst) &
             DL_I2C_CONTROLLER_STATUS_IDLE)) {
        if (guard-- == 0u) {
            return;
        }
    }

    DL_I2C_flushControllerTXFIFO(inst);

    /* 第 2 步：预载 FIFO（最多装满 FIFO） */
    loaded    = DL_I2C_fillControllerTXFIFO(inst, (uint8_t *)p, len);
    p        += loaded;
    remaining = (uint8_t)(len - (uint8_t)loaded);

    /* 第 3 步：启动传输，告知硬件总字节数 */
    DL_I2C_startControllerTransfer(inst, addr,
        DL_I2C_CONTROLLER_DIRECTION_TX, len);

    /* 第 4 步：对于长度超过 FIFO 的包，循环填入剩余字节 */
    while (remaining > 0u) {
        guard = timeout;
        while (DL_I2C_isControllerTXFIFOFull(inst)) {
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
        DL_I2C_transmitControllerData(inst, *p++);
        remaining--;
    }

    /* 第 5 步：等待传输完成 */
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
