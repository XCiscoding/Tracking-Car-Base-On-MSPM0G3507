#include "oled.h"
#include "ti_msp_dl_config.h"
#include "i2c_hal.h"
#include "font_5x7.h"
#include <string.h>

// ========== I2C 通信参数 ==========
#define OLED_ADDR           0x3C    // SSD1306 7-bit I2C address
#define OLED_CMD            0x00    // 控制字节: 命令
#define OLED_DATA           0x40    // 控制字节: 数据

#define WIDTH               128     // 屏幕宽度 (像素)
#define HEIGHT              64      // 屏幕高度 (像素)
#define PAGE_NUM            (HEIGHT / 8)  // 页数 (每页8行)

// 显存缓冲区：每页有 WIDTH 个字节，共 PAGE_NUM 页
static uint8_t oled_buffer[WIDTH * PAGE_NUM];

// ---------- 底层 I2C 写命令 / 写数据 ----------
static void oled_write_cmd(uint8_t cmd) {
    uint8_t buf[2] = {OLED_CMD, cmd};
    DL_I2C_Master_transmit(I2C_OLED_INST, OLED_ADDR, buf, 2, 100000);
}

static void oled_write_data(uint8_t data) {
    uint8_t buf[2] = {OLED_DATA, data};
    DL_I2C_Master_transmit(I2C_OLED_INST, OLED_ADDR, buf, 2, 100000);
}

// ---------- 显存操作 ----------
void OLED_Clear(void) {
    memset(oled_buffer, 0, sizeof(oled_buffer));
    OLED_Refresh();
}

void OLED_ClearLine(uint8_t y) {
    /* 清除 y 所在整页（128字节），不立即刷新屏幕 */
    uint8_t page = y / 8;
    if (page < PAGE_NUM)
        memset(&oled_buffer[page * WIDTH], 0, WIDTH);
}

void OLED_Refresh(void) {
    for (uint8_t page = 0; page < PAGE_NUM; page++) {
        // 设置页地址 (0xB0 + page)
        oled_write_cmd(0xB0 + page);
        // 设置列低4位地址 (低列地址)
        oled_write_cmd(0x00);
        // 设置列高4位地址 (高列地址)
        oled_write_cmd(0x10);
        // 发送该页所有列的数据
        for (uint8_t x = 0; x < WIDTH; x++) {
            oled_write_data(oled_buffer[page * WIDTH + x]);
        }
    }
}

// ---------- 字符显示 ----------
void OLED_ShowChar(uint8_t x, uint8_t y, char ch, uint8_t size) {
    // 超出屏幕范围则返回
    if (x > WIDTH - 8 || y > HEIGHT - 8) return;

    uint8_t c = ch - ' ';        // 字库索引
    if (c >= 96) return;         // 仅支持 ASCII 32~127

    /* 先清除字符占用的显存区域，防止旧像素残留（|= 只置位不清零）*/
    uint8_t char_w    = (size == 1) ? 6 : 12;
    uint8_t base_page = y / 8;
    uint8_t page_cnt  = (size == 1) ? 1 : 2;
    for (uint8_t p = base_page; p < base_page + page_cnt && p < PAGE_NUM; p++) {
        for (uint8_t col = 0; col < char_w && (x + col) < WIDTH; col++) {
            oled_buffer[p * WIDTH + x + col] = 0;
        }
    }

    if (size == 1) {  // 6x8 正常大小
        for (uint8_t i = 0; i < 5; i++) {
            uint8_t line = ascii_5x7[c][i];
            for (uint8_t j = 0; j < 8; j++) {
                if (line & (1 << j)) {
                    // 计算像素在显存中的位置
                    uint16_t index = (y + j) / 8 * WIDTH + x + i;
                    if (index < sizeof(oled_buffer))
                        oled_buffer[index] |= 1 << ((y + j) % 8);
                }
            }
        }
    } else if (size == 2) {  // 12x16 放大一倍
        for (uint8_t i = 0; i < 5; i++) {
            uint8_t line = ascii_5x7[c][i];
            for (uint8_t j = 0; j < 8; j++) {
                if (line & (1 << j)) {
                    // 放大2x2块
                    for (uint8_t dx = 0; dx < 2; dx++) {
                        for (uint8_t dy = 0; dy < 2; dy++) {
                            uint16_t index = (y + j*2 + dy) / 8 * WIDTH + x + i*2 + dx;
                            if (index < sizeof(oled_buffer))
                                oled_buffer[index] |= 1 << ((y + j*2 + dy) % 8);
                        }
                    }
                }
            }
        }
    }
}

void OLED_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size) {
    uint8_t cur_x = x, cur_y = y;
    while (*str) {
        OLED_ShowChar(cur_x, cur_y, *str++, size);
        cur_x += (size == 1) ? 6 : 12;   // 字符宽度
        if (cur_x > WIDTH - 6) {          // 换行
            cur_x = 0;
            cur_y += (size == 1) ? 8 : 16;
        }
    }
}

void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size) {
    char buf[16];
    uint8_t width = (len < (sizeof(buf) - 1)) ? len : (sizeof(buf) - 1);

    for (uint8_t i = 0; i < width; i++) {
        buf[width - 1 - i] = (char)('0' + (num % 10));
        num /= 10;
    }
    buf[width] = '\0';

    OLED_ShowString(x, y, buf, size);
}

void OLED_ShowFloat(uint8_t x, uint8_t y, float num, uint8_t int_len, uint8_t size) {
    uint32_t scaled;
    uint32_t integer;
    uint32_t fraction;
    uint8_t pos = x;

    if (num < 0.0f) {
        OLED_ShowChar(pos, y, '-', size);
        pos += (size == 1) ? 6 : 12;
        num = -num;
    }

    scaled = (uint32_t) ((num * 100.0f) + 0.5f);
    integer = scaled / 100u;
    fraction = scaled % 100u;

    OLED_ShowNum(pos, y, integer, int_len, size);
    pos += (uint8_t) (((size == 1) ? 6 : 12) * int_len);
    OLED_ShowChar(pos, y, '.', size);
    pos += (size == 1) ? 6 : 12;
    OLED_ShowNum(pos, y, fraction, 2, size);
}

// ---------- 初始化 ----------
bool OLED_Init(void) {
    // 上电等待稳定
    for (volatile uint32_t i = 0; i < 10000; i++);

    // SSD1306 初始化序列
    oled_write_cmd(0xAE); oled_write_cmd(0xD5); oled_write_cmd(0x80);
    oled_write_cmd(0xA8); oled_write_cmd(0x3F); oled_write_cmd(0xD3);
    oled_write_cmd(0x00); oled_write_cmd(0x40); oled_write_cmd(0x8D);
    oled_write_cmd(0x14); oled_write_cmd(0x20); oled_write_cmd(0x00);
    oled_write_cmd(0xA1); oled_write_cmd(0xC8); oled_write_cmd(0xDA);
    oled_write_cmd(0x12); oled_write_cmd(0x81); oled_write_cmd(0xCF);
    oled_write_cmd(0xD9); oled_write_cmd(0xF1); oled_write_cmd(0xDB);
    oled_write_cmd(0x40); oled_write_cmd(0xA4); oled_write_cmd(0xA6);
    oled_write_cmd(0xAF);

    OLED_Clear();
    return true;
}
