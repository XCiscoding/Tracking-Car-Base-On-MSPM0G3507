#include "buzzer.h"
#include "ti_msp_dl_config.h"
#include "motor.h"   /* delay_ms() */

void Buzzer_Init(void)
{
    /* GPIO 已在 SYSCFG_DL_GPIO_init() 配置为数字输出，初始 HIGH（不响）
     * 此处无需重复操作，保留接口供统一 Init 序列调用 */
}

void Buzzer_On(void)
{
    DL_GPIO_clearPins(GPIO_BUZZER_PORT, GPIO_BUZZER_PIN);
}

void Buzzer_Off(void)
{
    DL_GPIO_setPins(GPIO_BUZZER_PORT, GPIO_BUZZER_PIN);
}

void Buzzer_BeepMs(uint32_t ms)
{
    Buzzer_On();
    delay_ms(ms);
    Buzzer_Off();
}

void Buzzer_Beep(uint8_t n)
{
    uint8_t i;
    for (i = 0; i < n; i++) {
        Buzzer_BeepMs(100);
        if (i < n - 1u) {
            delay_ms(100);  /* 每声之间静音间隔 */
        }
    }
}
