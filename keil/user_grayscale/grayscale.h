#ifndef _GRAYSCALE_H_
#define _GRAYSCALE_H_

#include <stdint.h>

/** 初始化灰度传感器 (引脚已在 SysConfig 中配置) */
void Grayscale_Init(void);

/**
 * @brief 读取8路灰度值
 * @return 8位二进制数，bit7对应第1路(最左)，bit0对应第8路(最右)
 *         位为1表示白底，0表示黑线 (因为内部下拉，黑线输出低电平)
 */
uint8_t Grayscale_Read(void);

/**
 * @brief 计算当前小车相对于黑线的偏差
 * @return -100~100，负值表示偏左，正值表示偏右，0表示居中
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