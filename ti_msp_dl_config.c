/*
 * Copyright (c) 2023, Texas Instruments Incorporated
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

/*
 *  ============ ti_msp_dl_config.c =============
 *  Configured MSPM0 DriverLib module definitions
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */

#include "ti_msp_dl_config.h"

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform any initialization needed before using any board APIs
 */
SYSCONFIG_WEAK void SYSCFG_DL_init(void)
{
    SYSCFG_DL_initPower();
    SYSCFG_DL_GPIO_init();
    /* Module-Specific Initializations*/
    SYSCFG_DL_SYSCTL_init();
    SYSCFG_DL_I2C_OLED_init();
    SYSCFG_DL_PWM_LEFT_init();
    SYSCFG_DL_PWM_RIGHT_init();
}

SYSCONFIG_WEAK void SYSCFG_DL_initPower(void)
{
    DL_GPIO_reset(GPIOA);
    DL_GPIO_reset(GPIOB);
    DL_I2C_reset(I2C_OLED_INST);
    DL_TimerA_reset(PWM_LEFT_INST);
    DL_TimerA_reset(PWM_RIGHT_INST);
    DL_TimerG_reset(QEI_LEFT_INST);

    DL_GPIO_enablePower(GPIOA);
    DL_GPIO_enablePower(GPIOB);
    DL_I2C_enablePower(I2C_OLED_INST);
    DL_TimerA_enablePower(PWM_LEFT_INST);
    DL_TimerA_enablePower(PWM_RIGHT_INST);
    DL_TimerG_enablePower(QEI_LEFT_INST);
    delay_cycles(POWER_STARTUP_DELAY);
}

SYSCONFIG_WEAK void SYSCFG_DL_GPIO_init(void)
{
    DL_GPIO_initPeripheralInputFunctionFeatures(I2C_OLED_IOMUX_SDA,
        I2C_OLED_IOMUX_SDA_FUNC, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initPeripheralInputFunctionFeatures(I2C_OLED_IOMUX_SCL,
        I2C_OLED_IOMUX_SCL_FUNC, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_enableHiZ(I2C_OLED_IOMUX_SDA);
    DL_GPIO_enableHiZ(I2C_OLED_IOMUX_SCL);

    DL_GPIO_initDigitalOutput(GPIO_LED_PA7_IOMUX);

    /* 左电机方向引脚: AIN1=PA13, AIN2=PA14, 初始为 LOW (停止) */
    DL_GPIO_initDigitalOutput(MOTOR_AIN1_IOMUX);
    DL_GPIO_initDigitalOutput(MOTOR_AIN2_IOMUX);

    /* 左电机 PWM 引脚: PA8 -> TIMA0 CCP0 */
    DL_GPIO_initPeripheralOutputFunction(PWM_LEFT_IOMUX, PWM_LEFT_IOMUX_FUNC);

    /* 右电机方向引脚: BIN1=PA16(PINCM38), BIN2=PA17(PINCM39), 初始为 LOW */
    DL_GPIO_initDigitalOutput(MOTOR_BIN1_IOMUX);
    DL_GPIO_initDigitalOutput(MOTOR_BIN2_IOMUX);

    /* 右电机 PWM 引脚: PA24 -> TIMA1 CCP1 */
    DL_GPIO_initPeripheralOutputFunction(PWM_RIGHT_IOMUX, PWM_RIGHT_IOMUX_FUNC);

    DL_GPIO_clearPins(GPIO_LED_PORT, GPIO_LED_PA7_PIN);
    DL_GPIO_enableOutput(GPIO_LED_PORT, GPIO_LED_PA7_PIN);

    DL_GPIO_clearPins(MOTOR_AIN1_PORT,
        MOTOR_AIN1_PIN_0_PIN | MOTOR_AIN2_PIN_1_PIN);
    DL_GPIO_enableOutput(MOTOR_AIN1_PORT,
        MOTOR_AIN1_PIN_0_PIN | MOTOR_AIN2_PIN_1_PIN);

    DL_GPIO_clearPins(MOTOR_BIN1_PORT,
        MOTOR_BIN1_PIN_2_PIN | MOTOR_BIN2_PIN_3_PIN);
    DL_GPIO_enableOutput(MOTOR_BIN1_PORT,
        MOTOR_BIN1_PIN_2_PIN | MOTOR_BIN2_PIN_3_PIN);

}

static const DL_I2C_ClockConfig gI2C_OLEDClockConfig = {
    .clockSel    = DL_I2C_CLOCK_BUSCLK,
    .divideRatio = DL_I2C_CLOCK_DIVIDE_1,
};

SYSCONFIG_WEAK void SYSCFG_DL_I2C_OLED_init(void)
{
    DL_I2C_setClockConfig(I2C_OLED_INST,
        (DL_I2C_ClockConfig *) &gI2C_OLEDClockConfig);
    DL_I2C_setAnalogGlitchFilterPulseWidth(I2C_OLED_INST,
        DL_I2C_ANALOG_GLITCH_FILTER_WIDTH_50NS);
    DL_I2C_enableAnalogGlitchFilter(I2C_OLED_INST);

    DL_I2C_resetControllerTransfer(I2C_OLED_INST);
    /* 100 kHz from 32 MHz BUSCLK: 32 MHz / (10 * (1 + 31)). */
    DL_I2C_setTimerPeriod(I2C_OLED_INST, 31);
    DL_I2C_setControllerTXFIFOThreshold(I2C_OLED_INST,
        DL_I2C_TX_FIFO_LEVEL_BYTES_1);
    DL_I2C_setControllerRXFIFOThreshold(I2C_OLED_INST,
        DL_I2C_RX_FIFO_LEVEL_BYTES_1);
    DL_I2C_enableControllerClockStretching(I2C_OLED_INST);
    DL_I2C_enableController(I2C_OLED_INST);
}

/* ======== PWM LEFT: TIMA0, PA8, CC0, period=1000 ======== */
/* 32 MHz BUSCLK, prescale=0 -> PWM freq = 32MHz/1000 = 32 kHz */
static const DL_TimerA_ClockConfig gPWM_LEFT_ClockConfig = {
    .clockSel    = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
    .prescale    = 0U,
};

static const DL_TimerA_PWMConfig gPWM_LEFT_PWMConfig = {
    .period          = 1000U,
    .pwmMode         = DL_TIMER_PWM_MODE_EDGE_ALIGN,
    .isTimerWithFourCC = false,
    .startTimer      = DL_TIMER_START,
};

SYSCONFIG_WEAK void SYSCFG_DL_PWM_LEFT_init(void)
{
    DL_TimerA_setClockConfig(PWM_LEFT_INST,
        (DL_TimerA_ClockConfig *) &gPWM_LEFT_ClockConfig);
    DL_TimerA_initPWMMode(PWM_LEFT_INST,
        (DL_TimerA_PWMConfig *) &gPWM_LEFT_PWMConfig);
    /* 初始占空比为 0 (停止) */
    DL_TimerA_setCaptureCompareValue(PWM_LEFT_INST, 0, DL_TIMER_CC_0_INDEX);
    /* 使能 CC0 输出，输出源设为定时器功能值（PWM 波形） */
    DL_TimerA_setCaptureCompareOutCtl(PWM_LEFT_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
        DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
        DL_TIMER_CC_0_INDEX);
    DL_TimerA_setCCPDirection(PWM_LEFT_INST, DL_TIMER_CC0_OUTPUT);
    DL_TimerA_enableClock(PWM_LEFT_INST);
}

/* ======== PWM RIGHT: TIMA1, PA24, CC1, period=1000 ======== */
/* 32 MHz BUSCLK, prescale=0 -> PWM freq = 32MHz/1000 = 32 kHz */
static const DL_TimerA_ClockConfig gPWM_RIGHT_ClockConfig = {
    .clockSel    = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
    .prescale    = 0U,
};

static const DL_TimerA_PWMConfig gPWM_RIGHT_PWMConfig = {
    .period          = 1000U,
    .pwmMode         = DL_TIMER_PWM_MODE_EDGE_ALIGN,
    .isTimerWithFourCC = false,
    .startTimer      = DL_TIMER_START,
};

SYSCONFIG_WEAK void SYSCFG_DL_PWM_RIGHT_init(void)
{
    DL_TimerA_setClockConfig(PWM_RIGHT_INST,
        (DL_TimerA_ClockConfig *) &gPWM_RIGHT_ClockConfig);
    DL_TimerA_initPWMMode(PWM_RIGHT_INST,
        (DL_TimerA_PWMConfig *) &gPWM_RIGHT_PWMConfig);
    /* 初始占空比为 0 (停止) */
    DL_TimerA_setCaptureCompareValue(PWM_RIGHT_INST, 0, DL_TIMER_CC_1_INDEX);
    /* 使能 CC1 输出，输出源设为定时器功能值（PWM 波形） */
    DL_TimerA_setCaptureCompareOutCtl(PWM_RIGHT_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
        DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
        DL_TIMER_CC_1_INDEX);
    DL_TimerA_setCCPDirection(PWM_RIGHT_INST, DL_TIMER_CC1_OUTPUT);
    DL_TimerA_enableClock(PWM_RIGHT_INST);
}

SYSCONFIG_WEAK void SYSCFG_DL_SYSCTL_init(void)
{

	//Low Power Mode is configured to be SLEEP0
    DL_SYSCTL_setBORThreshold(DL_SYSCTL_BOR_THRESHOLD_LEVEL_0);

    DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE);
    /* Set default configuration */
    DL_SYSCTL_disableHFXT();
    DL_SYSCTL_disableSYSPLL();
    DL_SYSCTL_setULPCLKDivider(DL_SYSCTL_ULPCLK_DIV_1);
    DL_SYSCTL_setMCLKDivider(DL_SYSCTL_MCLK_DIVIDER_DISABLE);

}


