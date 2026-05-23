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
float    IMU_GetRelativeRoll(void);
float    IMU_GetRelativePitch(void);
float    IMU_GetRawYaw(void);
float    IMU_GetYawOffset(void);
float    IMU_GetRoll(void);
float    IMU_GetPitch(void);
float    IMU_GetGyroX(void);
float    IMU_GetGyroY(void);
float    IMU_GetGyroZ(void);
void     IMU_SendGyroOnlyConfig(void);
uint32_t IMU_GetConfigSendCount(void);
void     IMU_ResetYaw(void);
void     IMU_ResetAngles(void);
bool     IMU_IsDataReady(void);
uint32_t IMU_GetRawByteCount(void);              /* 诊断：ISR 收到的原始字节总数 */
uint8_t  IMU_GetRawCapture(uint8_t *buf, uint8_t n); /* 诊断：最近 n 个字节（滚动） */
uint8_t  IMU_GetLastFrame(uint8_t *buf, uint8_t n);  /* 诊断：最近一帧校验通过的完整 0x55 帧 */
int16_t  IMU_GetCmd55Value(uint8_t index);           /* 诊断：CMD=0x01 payload 的第 index 个 int16 */
uint32_t IMU_GetCmd55Count(void);                    /* 诊断：CMD=0x01 有效帧计数 */
int16_t  IMU_GetCmd3Value(uint8_t index);            /* 诊断：CMD=0x03 payload 的第 index 个 int16 */
uint32_t IMU_GetCmd3Count(void);                     /* 诊断：CMD=0x03 有效帧计数 */
int16_t  IMU_GetHeadingGyroRaw(void);                /* 诊断：当前暂定航向候选值，来自 CMD=0x02 的 G0 零偏 */
uint32_t IMU_GetCksumOkCount(void);              /* 诊断：checksum 通过的帧计数（任意 CMD） */
uint32_t IMU_Get55Count(void);                   /* 诊断：0x55 帧头字节出现次数 */
uint8_t  IMU_GetLastCmd(void);                   /* 诊断：最后一个 CMD 字节（0x53=欧拉角帧） */

#endif /* IMU_H */
