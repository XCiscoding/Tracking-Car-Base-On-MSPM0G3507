#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

/**
 * 蜂鸣器驱动 —— PB17(PINCM43)，有源蜂鸣器，低电平触发
 * GPIO 已在 SYSCFG_DL_GPIO_init() 中配置好，Buzzer_Init() 无需额外操作
 */

void Buzzer_Init(void);           /* 空占位，保持接口一致 */
void Buzzer_On(void);             /* 开始鸣叫  (PB17 拉低) */
void Buzzer_Off(void);            /* 停止鸣叫  (PB17 拉高) */
void Buzzer_BeepMs(uint32_t ms);  /* 鸣叫 ms 毫秒后停止 (阻塞) */
void Buzzer_Beep(uint8_t n);      /* 短响 n 声，每声 100ms，间隔 100ms (阻塞) */

#endif /* BUZZER_H */
