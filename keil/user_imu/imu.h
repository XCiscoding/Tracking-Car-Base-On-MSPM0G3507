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

void  IMU_Init(void);          /* 复位内部状态；UART1 已在 SYSCFG 中启动 */
float IMU_GetYaw(void);        /* 收益偵訪角 (度)，相对上次 ResetYaw 的增量 */
float IMU_GetRoll(void);       /* 横滚角 (度) */
float IMU_GetPitch(void);      /* 俧仰角 (度) */
void  IMU_ResetYaw(void);      /* 清零偶转角基准 —— 进入直线段前调用 */
bool  IMU_IsDataReady(void);   /* 是否有新帧数据（读后自动清零标志） */

#endif /* IMU_H */
