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

/* Keil's MSPM0 flash algorithm requires the programmed image length to be
 * 8-byte aligned. Keep 4 bytes in flash so the current image does not end
 * with a short 4-byte ProgramPage transfer.
 */
__attribute__((used)) const uint32_t g_flash_program_padding[3] = {
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu
};

/*
 * 显示左右轮独立脉冲数 + 均值，每 200 ms 刷新一次。
 * 手拨车轮，若对应行数值增加说明该路编码器 ISR 正常。
 */
static void oled_show_encoder(void)
{
    OLED_ClearLine(8);
    OLED_ShowString(0, 8,  "L:", 1);
    OLED_ShowNum(12, 8,  Encoder_GetLeftPulse(),  6, 1);

    OLED_ClearLine(16);
    OLED_ShowString(0, 16, "R:", 1);
    OLED_ShowNum(12, 16, Encoder_GetRightPulse(), 6, 1);

    OLED_ClearLine(24);
    OLED_ShowString(0, 24, "Avg:", 1);
    OLED_ShowNum(30, 24, (uint32_t)Encoder_GetPulse(), 5, 1);

    OLED_Refresh();
}

int main(void)
{
    SYSCFG_DL_init();
    Motor_Init();

    delay_ms(100);
    OLED_Init();
    OLED_ShowString(0, 0, "Enc Debug", 1);
    OLED_Refresh();

    DL_GPIO_setPins(GPIO_LED_PORT, GPIO_LED_PA7_PIN);

    Encoder_Reset();    /* 从 0 开始 */

    /* ── 纯手拨测试：不启动电机，实时刷新 OLED ──
     * L: 左轮脉冲数   R: 右轮脉冲数
     * 手拨某个轮，对应行数值增加 → 该路 ISR 正常
     * 两行都不变 → 检查编码器接线（PA12 / PB16）
     */
    while (1) {
        oled_show_encoder();
        delay_ms(200);
    }
}
