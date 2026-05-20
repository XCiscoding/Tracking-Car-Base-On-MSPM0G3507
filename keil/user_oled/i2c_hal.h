#ifndef _I2C_HAL_H_
#define _I2C_HAL_H_

#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>

/**
 * @brief 阻塞式 I2C 主机发送（对 oled.c 的适配层）
 * @param inst   I2C 外设实例，如 I2C0
 * @param addr   7 位从机地址
 * @param buf    数据缓冲区指针
 * @param len    发送字节数
 * @param timeout 超时（当前实现为忙等，参数保留）
 */
void DL_I2C_Master_transmit(I2C_Regs *inst, uint8_t addr,
                             const uint8_t *buf, uint8_t len,
                             uint32_t timeout);

#endif /* _I2C_HAL_H_ */
