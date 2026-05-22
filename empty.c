/*
 * Copyright (c) 2021, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ti_msp_dl_config.h"
#include "oled.h"
#include "motor.h"
#include "imu.h"

/* Keil's MSPM0 flash algorithm requires the programmed image length to be
 * 8-byte aligned. Keep 4 bytes in flash so the current image does not end
 * with a short 4-byte ProgramPage transfer.
 */
__attribute__((used)) const uint32_t g_flash_program_padding[3] = {
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu
};

/*
 * 在 OLED 上以十六进制显示一个字节，格式 "HH"（2字符，各 6px 宽）
 */
static void oled_show_hex(uint8_t x, uint8_t y, uint8_t val)
{
    const char hex[] = "0123456789ABCDEF";
    OLED_ShowChar(x,     y, hex[val >> 4],  1);
    OLED_ShowChar(x + 6, y, hex[val & 0xF], 1);
}

/*
 * 在 OLED 上显示一个带符号的定点数（value = 实际值 × 10）
 * 格式：[-]DDD.D，占 5 个字符宽度（6×5 像素字体）
 * x,y：左上角坐标；size=1（6×8）
 */
static void oled_show_fixed1(uint8_t x, uint8_t y, int32_t val10)
{
    uint32_t abs_val = (uint32_t)(val10 < 0 ? -val10 : val10);
    uint32_t intpart = abs_val / 10;
    uint32_t frac    = abs_val % 10;

    if (val10 < 0) {
        OLED_ShowChar(x, y, '-', 1);
    } else {
        OLED_ShowChar(x, y, ' ', 1);
    }
    OLED_ShowNum(x + 6,  y, intpart, 3, 1);   /* 3位整数部分 */
    OLED_ShowChar(x + 24, y, '.', 1);
    OLED_ShowNum(x + 30, y, frac,    1, 1);   /* 1位小数 */
}

int main(void)
{
    uint32_t last_update        = 0;
    uint32_t last_angle_refresh = 0;
    uint32_t frame_cnt          = 0;   /* 收到帧计数，用于确认数据在流动 */

    SYSCFG_DL_init();
    Motor_Init();
    delay_ms(600);   /* 等 IMU 上电稳定，IMU_Init 内会发配置命令 */
    IMU_Init();
    OLED_Init();

    /* 固定标签（y 对齐到 page 边界，每行间距 8px） */
    OLED_ShowString(0,  0, "Yaw:", 1);
    OLED_ShowString(0,  8, "Rol:", 1);
    OLED_Refresh();

    while (1) {
        if (IMU_IsDataReady()) {
            frame_cnt++;

            int32_t yaw10 = (int32_t)(IMU_GetYaw()   * 10.0f);
            int32_t rol10 = (int32_t)(IMU_GetRoll()  * 10.0f);

            oled_show_fixed1(24,  0, yaw10);
            oled_show_fixed1(24,  8, rol10);

            /* 角度行每 100ms 刷新一次（10Hz），转动 IMU 可实时跟踪 */
            uint32_t t = Motor_GetTickMs();
            if ((t - last_angle_refresh) >= 100u) {
                last_angle_refresh = t;
                OLED_Refresh();
            }
        }

        /* 每 500ms 刷新一次字节/帧计数 + 前4字节 Hex */
        uint32_t now = Motor_GetTickMs();
        if ((now - last_update) >= 500) {
            last_update = now;

            uint32_t raw = IMU_GetRawByteCount();

            /* 第3行(page2, y=16)：B:XXXXX F:XXXX C:XXXX */
            OLED_ClearLine(16);
            OLED_ShowString(0,  16, "B:", 1);
            OLED_ShowNum(12, 16, raw,                       5, 1);
            OLED_ShowString(42, 16, "F:", 1);
            OLED_ShowNum(54, 16, frame_cnt,                 4, 1);
            OLED_ShowString(78, 16, "C:", 1);
            OLED_ShowNum(90, 16, IMU_GetCksumOkCount(),     4, 1);

            /* 第4行(page3, y=24)：5: = 0x55计数  K: = 最后CMD字节(hex) */
            OLED_ClearLine(24);
            OLED_ShowString(0,  24, "5:", 1);
            OLED_ShowNum(12, 24, IMU_Get55Count(),  5, 1);
            OLED_ShowString(48, 24, "K:", 1);
            oled_show_hex(60, 24, IMU_GetLastCmd());

            /* 第5行(page4, y=32)：最近8字节原始数据前4字节 HH HH HH HH */
            uint8_t cap[8] = {0};
            IMU_GetRawCapture(cap, 8);
            OLED_ClearLine(32);
            oled_show_hex(0,  32, cap[0]);  oled_show_hex(16, 32, cap[1]);
            oled_show_hex(32, 32, cap[2]);  oled_show_hex(48, 32, cap[3]);
            oled_show_hex(64, 32, cap[4]);  oled_show_hex(80, 32, cap[5]);
            oled_show_hex(96, 32, cap[6]);  oled_show_hex(112,32, cap[7]);

            OLED_Refresh();
        }
    }
}
