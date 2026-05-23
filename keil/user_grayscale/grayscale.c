#include "grayscale.h"
#include "motor.h"

static const int16_t weight[6] = {-5, -3, -1, 1, 3, 5};

void Grayscale_Init(void) {
    /* Yahboom 8路巡线模块只接 X1/X2/X4/X5/X7/X8，GPIO 已在 SYSCFG_DL_GPIO_init() 初始化为上拉输入。 */
}

uint8_t Grayscale_Read(void) {
    uint8_t val = 0;

    if (DL_GPIO_readPins(GRAY_X1_PORT, GRAY_X1_PIN) == 0) val |= 0x20;
    if (DL_GPIO_readPins(GRAY_X2_PORT, GRAY_X2_PIN) == 0) val |= 0x10;
    if (DL_GPIO_readPins(GRAY_X4_PORT, GRAY_X4_PIN) == 0) val |= 0x08;
    if (DL_GPIO_readPins(GRAY_X5_PORT, GRAY_X5_PIN) == 0) val |= 0x04;
    if (DL_GPIO_readPins(GRAY_X7_PORT, GRAY_X7_PIN) == 0) val |= 0x02;
    if (DL_GPIO_readPins(GRAY_X8_PORT, GRAY_X8_PIN) == 0) val |= 0x01;
    return val;
}

int16_t Grayscale_GetError(void) {
    uint8_t raw = Grayscale_Read();
    int16_t error = 0, sum_weight = 0;
    static int16_t last_error = 0;

    for (int i = 0; i < 6; i++) {
        if ((raw >> (5 - i)) & 0x01) {
            error += weight[i];
            sum_weight++;
        }
    }

    if (sum_weight == 0) {
        return last_error;
    }

    error = error * 100 / sum_weight;
    if (error > 100) error = 100;
    if (error < -100) error = -100;
    last_error = error;
    return error;
}

void Line_Follow(uint16_t base_speed, float kp) {
    int16_t err = Grayscale_GetError();
    int16_t correction = (int16_t)(kp * err);
    /* Motor_SetDifferential 内部已处理限幅 (-1000~1000) 和方向 GPIO */
    Motor_SetDifferential((int16_t)base_speed - correction,
                          (int16_t)base_speed + correction);
}
