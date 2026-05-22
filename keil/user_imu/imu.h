#ifndef IMU_H
#define IMU_H

#include <stdint.h>
#include <stdbool.h>

/**
 * ATK-IMU901 驱动 —— UART1 中断接收 + 帧解析
 *
 * 帧格式: [0xAA][0xFF][CMD][LEN][DATA...][CKSUM]
 *   CMD=0x44, LEN=6: 欧拉角 Roll/Pitch/Yaw (int16, ×0.01 度, 小端)
 *   CKSUM = (CMD+LEN+所有DATA字节之和) & 0xFF
 *
 * 引脚: MCU UART1_TX=PB6(PINCM23), UART1_RX=PB7(PINCM24)
 *        PB7(RX) 接收 IMU 输出的欧拉角数据帧
 *
 * 用法:
 *   IMU_Init();
 *   while(1) {
 *       if (IMU_IsDataReady()) { float yaw = IMU_GetYaw(); ... }
 *   }
 */

void     IMU_Init(void);
float    IMU_GetYaw(void);
float    IMU_GetRoll(void);
float    IMU_GetPitch(void);
void     IMU_ResetYaw(void);
bool     IMU_IsDataReady(void);
uint32_t IMU_GetRawByteCount(void);              /* 诊断：ISR 收到的原始字节总数 */
uint8_t  IMU_GetRawCapture(uint8_t *buf, uint8_t n); /* 诊断：最近 n 个字节（滚动） */
uint32_t IMU_GetCksumOkCount(void);              /* 诊断：checksum 通过的帧计数（任意 CMD） */
uint32_t IMU_Get55Count(void);                   /* 诊断：0x55 帧头字节出现次数 */
uint8_t  IMU_GetLastCmd(void);                   /* 诊断：最后一个 CMD 字节（0x53=欧拉角帧） */

#endif /* IMU_H */
