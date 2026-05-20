#ifndef _OLED_H_
#define _OLED_H_

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 初始化 OLED 显示屏 (I2C 接口)
 * @return true 成功, false 失败
 */
bool OLED_Init(void);

/** 清空整个屏幕（清缓冲并立即刷新） */
void OLED_Clear(void);

/**
 * @brief 清除一整行（y 所在的 8px 页），仅清缓冲，不刷新屏幕
 * @param y 行坐标，应为 8 的倍数 (0,8,16,24,...)
 */
void OLED_ClearLine(uint8_t y);

/**
 * @brief 在指定位置显示一个字符
 * @param x 列坐标 (0~127)
 * @param y 行坐标 (0~7, 每行8像素)
 * @param ch 要显示的字符
 * @param size 字体大小: 1=6x8, 2=12x16
 */
void OLED_ShowChar(uint8_t x, uint8_t y, char ch, uint8_t size);

/**
 * @brief 显示字符串
 * @param x 起始列
 * @param y 起始行
 * @param str 字符串指针
 * @param size 字体大小
 */
void OLED_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size);

/**
 * @brief 显示无符号整数 (自动转十进制)
 * @param x 列
 * @param y 行
 * @param num 要显示的数字
 * @param len 最小位数 (不足时前面补空格)
 * @param size 字体大小
 */
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size);

/**
 * @brief 显示浮点数 (小数固定2位)
 * @param x 列
 * @param y 行
 * @param num 浮点数
 * @param int_len 整数部分显示位数 (不足补空格)
 * @param size 字体大小
 */
void OLED_ShowFloat(uint8_t x, uint8_t y, float num, uint8_t int_len, uint8_t size);

/** 将显存内容刷新到屏幕 (如果使用双缓冲) */
void OLED_Refresh(void);

#endif