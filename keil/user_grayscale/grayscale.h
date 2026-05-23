#ifndef _GRAYSCALE_H_
#define _GRAYSCALE_H_

#include <stdint.h>
#include "ti_msp_dl_config.h"

/** 初始化亚博八路巡线模块的 6 路输入；实际使用 X1/X2/X4/X5/X7/X8，X3/X6 不接。 */
void Grayscale_Init(void);

/**
 * @brief 读取6路灰度值
 * @return bit5~bit0 分别对应 X1/X2/X4/X5/X7/X8；1 表示检测到黑线，0 表示未检测到黑线
 */
uint8_t Grayscale_Read(void);

/**
 * @brief 计算当前小车相对于黑线的偏差
 * @return -100~100，负值表示黑线在左侧，正值表示黑线在右侧，0表示居中
 */
int16_t Grayscale_GetError(void);

/**
 * @brief 比例循迹控制
 * @param base_speed 基础速度 (0~1000)
 * @param kp         比例系数 (建议 0.5~2.0)
 *
 * 内部通过 Motor_SetDifferential() 差速控制，方向和限幅均已处理。
 */
void Line_Follow(uint16_t base_speed, float kp);

#endif
