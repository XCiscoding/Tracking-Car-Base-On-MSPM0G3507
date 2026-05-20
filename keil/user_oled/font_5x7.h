#ifndef _FONT_5X7_H_
#define _FONT_5X7_H_

#include <stdint.h>

/**
 * @brief 5x7 点阵 ASCII 字符集 (32~127)
 * 
 * 每个字符占用 5 个字节，每个字节的 8 个 bit 表示一列的 8 个像素点。
 * 数组大小为 [96][5]，索引 0 对应空格 ' ' (ASCII 32)，索引 95 对应 'z' (ASCII 122)。
 */
extern const uint8_t ascii_5x7[96][5];

#endif