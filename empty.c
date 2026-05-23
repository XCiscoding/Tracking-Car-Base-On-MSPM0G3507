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
#include "grayscale.h"

#define IMU_DIAG_REFRESH_MS     200u
#define STRAIGHT_TARGET_MM      500.0f
#define STRAIGHT_BASE_PWM       200
#define STRAIGHT_LEFT_TRIM      0
#define STRAIGHT_RIGHT_TRIM     0
#define STRAIGHT_ENC_KP         4
#define STRAIGHT_ENC_MAX_CORR   120
#define STRAIGHT_D0_DIV         20
#define STRAIGHT_D0_MAX_CORR    80
#define STRAIGHT_D0_VALID_LIMIT 3000
#define STRAIGHT_LOOP_MS        20u
#define STRAIGHT_SOFT_START_MS  150u
#define STRAIGHT_SOFT_PWM       170
#define STRAIGHT_MAX_RUN_MS     3000u

/*
 * 保留 8 字节对齐保护：Keil 的 MSPM0 Flash Algorithm 对镜像尾部长度敏感，
 * 这个常量用于避免 ProgramPage 最后一包出现短写入。
 */
__attribute__((used)) const uint32_t g_flash_program_padding[3] = {
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu
};

/*
 * 在 OLED 指定位置显示 1 个十六进制字节。
 * 用于把 IMU 串口原始字节直接显示出来，避免继续凭模块名猜协议。
 */
static void oled_show_hex_byte(uint8_t x, uint8_t y, uint8_t value)
{
    const char *hex = "0123456789ABCDEF";

    OLED_ShowChar(x, y, hex[(value >> 4) & 0x0Fu], 1);
    OLED_ShowChar((uint8_t)(x + 6u), y, hex[value & 0x0Fu], 1);
}

/*
 * 显示最近捕获的原始串口字节，用于确认 ATK-IMU901 实际帧头和命令字。
 * 输入来自 IMU 驱动的最近接收缓存；输出为 OLED 上的十六进制字节。
 */
static void oled_show_raw_bytes(void)
{
    uint8_t raw[16];
    uint8_t n = IMU_GetRawCapture(raw, 16u);

    OLED_ClearLine(0);
    OLED_ShowString(0, 0, "RAW", 1);
    OLED_ShowNum(24, 0, IMU_GetRawByteCount(), 5, 1);

    OLED_ClearLine(8);
    for (uint8_t i = 0; i < n && i < 4u; i++) {
        oled_show_hex_byte((uint8_t)(i * 30u), 8, raw[i]);
    }

    OLED_ClearLine(16);
    for (uint8_t i = 4u; i < n && i < 8u; i++) {
        oled_show_hex_byte((uint8_t)((i - 4u) * 30u), 16, raw[i]);
    }

    OLED_ClearLine(24);
    for (uint8_t i = 8u; i < n && i < 12u; i++) {
        oled_show_hex_byte((uint8_t)((i - 8u) * 30u), 24, raw[i]);
    }

    OLED_ClearLine(32);
    for (uint8_t i = 12u; i < n && i < 16u; i++) {
        oled_show_hex_byte((uint8_t)((i - 12u) * 30u), 32, raw[i]);
    }

    OLED_ClearLine(40);
    OLED_ShowString(0, 40, "C:", 1);
    OLED_ShowNum(18, 40, IMU_GetLastCmd(), 3, 1);
    OLED_ShowString(48, 40, "OK:", 1);
    OLED_ShowNum(72, 40, IMU_GetCksumOkCount(), 5, 1);

    OLED_Refresh();
}

/*
 * 在 OLED 指定行显示角度整数。
 * 显示值 = 实际角度 × 10，用于手动转车时判断哪一轴对应水平航向。
 */
static void oled_show_angle10(uint8_t y, const char *label, float angle)
{
    int32_t v = (int32_t)(angle * 10.0f);

    OLED_ShowString(0, y, label, 1);
    if (v < 0) {
        OLED_ShowChar(18, y, '-', 1);
        v = -v;
    } else {
        OLED_ShowChar(18, y, '+', 1);
    }
    OLED_ShowNum(30, y, (uint32_t)v, 5, 1);
}

/*
 * 在 OLED 指定行显示有符号整数。
 * 用于直接观察 CMD=0x01/0x02 的原始通道，确认哪个通道对应水平旋转。
 */
static void oled_show_i16(uint8_t y, const char *label, int16_t value)
{
    int32_t v = value;

    OLED_ShowString(0, y, label, 1);
    if (v < 0) {
        OLED_ShowChar(18, y, '-', 1);
        v = -v;
    } else {
        OLED_ShowChar(18, y, '+', 1);
    }
    OLED_ShowNum(30, y, (uint32_t)v, 5, 1);
}

/*
 * 同屏显示 CMD=0x02 和 CMD=0x03 的候选通道。
 * 目标是找出右转/左转符号相反、回正接近 0 的真实水平航向通道。
 */
static void oled_show_angles(void)
{
    OLED_ClearLine(0);
    oled_show_i16(0, "D0:", IMU_GetHeadingGyroRaw());

    OLED_ClearLine(8);
    oled_show_i16(8, "G1:", (int16_t)IMU_GetGyroY());

    OLED_ClearLine(16);
    oled_show_i16(16, "G2:", (int16_t)IMU_GetGyroZ());

    OLED_ClearLine(24);
    oled_show_i16(24, "C0:", IMU_GetCmd3Value(0));

    OLED_ClearLine(32);
    oled_show_i16(32, "C1:", IMU_GetCmd3Value(1));

    OLED_ClearLine(40);
    OLED_ShowString(0, 40, "C:", 1);
    OLED_ShowNum(18, 40, IMU_GetLastCmd(), 3, 1);
    OLED_ShowString(48, 40, "OK:", 1);
    OLED_ShowNum(72, 40, IMU_GetCksumOkCount(), 5, 1);

    OLED_Refresh();
}

static int16_t clamp_i16(int32_t value, int32_t min_value, int32_t max_value)
{
    if (value < min_value) return (int16_t)min_value;
    if (value > max_value) return (int16_t)max_value;
    return (int16_t)value;
}

static void oled_show_gray_bits(uint8_t x, uint8_t y, uint8_t raw)
{
    for (uint8_t i = 0u; i < 6u; i++) {
        uint8_t mask = (uint8_t)(0x20u >> i);
        OLED_ShowChar((uint8_t)(x + i * 6u), y, (raw & mask) ? '1' : '0', 1);
    }
}

static uint8_t grayscale_count_black(uint8_t raw)
{
    uint8_t n = 0u;

    for (uint8_t i = 0u; i < 6u; i++) {
        if (raw & (uint8_t)(0x20u >> i)) n++;
    }
    return n;
}

/*
 * 灰度静态诊断入口：只显示原始 6 路状态、黑线数量和偏差，不启动电机。
 * 用于先验收亚博八路巡线模块的接线、电平方向、bit 顺序和左右误差方向。
 */
static void Run_Grayscale_Diag(void)
{
    uint32_t last_oled_ms = 0u;

    Motor_Stop();
    Grayscale_Init();

    while (1) {
        uint32_t now = Motor_GetTickMs();

        if ((now - last_oled_ms) >= 100u) {
            uint8_t raw = Grayscale_Read();
            int16_t error = Grayscale_GetError();

            last_oled_ms = now;
            OLED_ClearLine(0);
            OLED_ShowString(0, 0, "GRAY DIAG", 1);
            OLED_ClearLine(8);
            OLED_ShowString(0, 8, "G:", 1);
            oled_show_gray_bits(18, 8, raw);
            OLED_ClearLine(16);
            oled_show_i16(16, "E:", error);
            OLED_ClearLine(24);
            OLED_ShowString(0, 24, "N:", 1);
            OLED_ShowNum(18, 24, grayscale_count_black(raw), 1, 1);
            OLED_ClearLine(32);
            OLED_ShowString(0, 32, "X1 X2 X4 X5 X7 X8", 1);
            OLED_Refresh();
        }
    }
}

/*
 * 用 D0 作为车头航向误差，叠加在原来的编码器左右轮同步修正上。
 * 输入为当前 D0 和左右编码器差；输出为左右轮 PWM，用于验证 D0 能否把 500mm 直线拉直。
 */
static void Run_D0_Straight_Test(void)
{
    uint32_t last_oled_ms = 0u;
    uint32_t last_ctrl_ms = 0u;
    int16_t last_enc_corr = 0;
    int16_t last_d0_corr = 0;
    uint32_t launch_start_ms;
    int16_t target_d0 = 0;
    uint8_t control_mode = 0u;

    Encoder_Reset();
    IMU_ResetAngles();
    target_d0 = IMU_GetHeadingGyroRaw();
    launch_start_ms = Motor_GetTickMs();

    while ((Encoder_GetDistanceMM() < STRAIGHT_TARGET_MM) &&
           ((Motor_GetTickMs() - launch_start_ms) < STRAIGHT_MAX_RUN_MS)) {
        uint32_t now = Motor_GetTickMs();
        (void)IMU_IsDataReady();

        if ((now - last_ctrl_ms) >= STRAIGHT_LOOP_MS) {
            int32_t left_pulse = (int32_t)Encoder_GetLeftPulse();
            int32_t right_pulse = (int32_t)Encoder_GetRightPulse();
            int32_t enc_error = left_pulse - right_pulse;
            int32_t enc_corr = enc_error * STRAIGHT_ENC_KP;
            int32_t heading_error = 0;
            int32_t base_pwm = STRAIGHT_BASE_PWM;
            int32_t left_pwm;
            int32_t right_pwm;
            bool soft_start_done = ((now - launch_start_ms) >= STRAIGHT_SOFT_START_MS);

            last_ctrl_ms = now;
            last_enc_corr = clamp_i16(enc_corr, -STRAIGHT_ENC_MAX_CORR, STRAIGHT_ENC_MAX_CORR);

            if (!soft_start_done) {
                control_mode = 0u;
                base_pwm = STRAIGHT_SOFT_PWM;
                last_d0_corr = 0;
            } else {
                control_mode = 1u;
                heading_error = (int32_t)IMU_GetHeadingGyroRaw() - (int32_t)target_d0;
                if (heading_error > STRAIGHT_D0_VALID_LIMIT || heading_error < -STRAIGHT_D0_VALID_LIMIT) {
                    last_d0_corr = 0;
                } else {
                    last_d0_corr = clamp_i16(-heading_error / STRAIGHT_D0_DIV,
                                             -STRAIGHT_D0_MAX_CORR,
                                             STRAIGHT_D0_MAX_CORR);
                }
            }

            left_pwm = base_pwm + STRAIGHT_LEFT_TRIM - last_enc_corr - last_d0_corr;
            right_pwm = base_pwm + STRAIGHT_RIGHT_TRIM + last_enc_corr + last_d0_corr;
            Motor_SetDifferential(clamp_i16(left_pwm, 0, 1000), clamp_i16(right_pwm, 0, 1000));
        }

        if ((now - last_oled_ms) >= IMU_DIAG_REFRESH_MS) {
            int32_t left_pulse = (int32_t)Encoder_GetLeftPulse();
            int32_t right_pulse = (int32_t)Encoder_GetRightPulse();

            last_oled_ms = now;
            OLED_ClearLine(0);
            oled_show_i16(0, "D0:", IMU_GetHeadingGyroRaw());
            OLED_ClearLine(8);
            oled_show_i16(8, "E:", clamp_i16(left_pulse - right_pulse, -32768, 32767));
            OLED_ClearLine(16);
            oled_show_i16(16, "EC:", last_enc_corr);
            OLED_ClearLine(24);
            oled_show_i16(24, "HC:", last_d0_corr);
            OLED_ClearLine(32);
            OLED_ShowString(0, 32, "M:", 1);
            OLED_ShowNum(18, 32, control_mode, 1, 1);
            OLED_ShowString(36, 32, "D:", 1);
            OLED_ShowNum(54, 32, (uint32_t)Encoder_GetDistanceMM(), 4, 1);
            OLED_ClearLine(40);
            OLED_ShowString(0, 40, "LP:", 1);
            OLED_ShowNum(24, 40, Motor_GetLeftDuty(), 3, 1);
            OLED_ShowString(60, 40, "RP:", 1);
            OLED_ShowNum(84, 40, Motor_GetRightDuty(), 3, 1);
            OLED_Refresh();
        }
    }

    Motor_Stop();
}

/*
 * ATK-IMU901 姿态角诊断入口。
 * 先等到第一帧有效数据再清零，随后手动水平转动车身验证哪一轴对应航向角。
 */
static void Run_IMU_ATK_Diag(void)
{
    uint32_t start_ms;
    uint32_t last_oled_ms = 0u;

    Motor_Stop();

    start_ms = Motor_GetTickMs();
    while (!IMU_IsDataReady()) {
        if ((Motor_GetTickMs() - start_ms) > 1500u) break;
    }
    IMU_ResetAngles();

    while (1) {
        uint32_t now = Motor_GetTickMs();
        (void)IMU_IsDataReady();

        if ((now - last_oled_ms) >= IMU_DIAG_REFRESH_MS) {
            last_oled_ms = now;
            oled_show_angles();
        }
    }
}

/*
 * IMU 姿态角诊断模式入口。
 * 当前不让电机运动，先确认 ATK-IMU901 的 Yaw 是否能作为直线航向反馈。
 */
int main(void)
{
    SYSCFG_DL_init();
    Motor_Init();
    delay_ms(600);
    IMU_Init();
    OLED_Init();

    OLED_ShowString(6, 0, "GRAY DIAG", 1);
    OLED_ShowString(6, 8, "X1X2X4X5X7X8", 1);
    OLED_ShowString(6, 16, "Motor stopped", 1);
    OLED_Refresh();
    delay_ms(800);

    Run_Grayscale_Diag();

    while (1) {}
}
