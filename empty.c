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
 * 统一刷新电机状态到 OLED：
 *   Row 2 (y=16): "L:xxxx R:xxxx"  —— 实时占空比
 *   Row 3 (y=24): status            —— 固定 7 字符宽，补空格覆盖残留像素
 */
static void oled_show_motor(const char *status)
{
    OLED_ShowString(0, 16, "L:    R:    ", 1);
    OLED_ShowNum(12, 16, Motor_GetLeftDuty(),  4, 1);
    OLED_ShowNum(54, 16, Motor_GetRightDuty(), 4, 1);
    OLED_ClearLine(24);             /* 清除整行，防止新字符串比旧字符串短时残留像素 */
    OLED_ShowString(0, 24, status, 1);
    OLED_Refresh();
}

int main(void)
{
    SYSCFG_DL_init();
    Motor_Init();       /* 启动 SysTick，初始化电机（停止状态） */

    delay_ms(100);      /* 等待 OLED 供电稳定 */
    OLED_Init();
    OLED_ShowString(0, 0, "Hello, ZM!", 1);
    OLED_Refresh();

    DL_GPIO_setPins(GPIO_LED_PORT, GPIO_LED_PA7_PIN);   /* LED 亮，确认程序运行 */

    /* 双轮测试：两轮各 50%，持续 2 秒后停止 */
    Motor_SetDifferential(500, 500);
    oled_show_motor("RUNNING");     /* "RUNNING" = 7 chars */

    delay_ms(2000);
    Motor_Stop();
	OLED_Refresh();
    oled_show_motor("STOP");

    while (1) {
    }
}
