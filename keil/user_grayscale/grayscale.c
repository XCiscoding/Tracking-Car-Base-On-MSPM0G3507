#include "grayscale.h"
#include "motor.h"

// 8路传感器的权重 (从左到右)
// 数值越大代表越偏离中心，用于计算加权偏差
static const int16_t weight[8] = {-7, -5, -3, -1, 1, 3, 5, 7};

void Grayscale_Init(void) {
    // 所有引脚已在 SysConfig 中配置为输入下拉，无需额外操作
}

uint8_t Grayscale_Read(void) {
    uint8_t val = 0;
    // 依次读取8个 GPIO 电平，当为 0 时（黑线）将对应位置1
    if (DL_GPIO_readPins(GREY_1_PORT, GREY_1_PIN_4_PIN) == 0) val |= 0x80;
    if (DL_GPIO_readPins(GREY_2_PORT, GREY_2_PIN_5_PIN) == 0) val |= 0x40;
    if (DL_GPIO_readPins(GREY_3_PORT, GREY_3_PIN_6_PIN) == 0) val |= 0x20;
    if (DL_GPIO_readPins(GREY_4_PORT, GREY_4_PIN_7_PIN) == 0) val |= 0x10;
    if (DL_GPIO_readPins(GREY_5_PORT, GREY_5_PIN_8_PIN) == 0) val |= 0x08;
    if (DL_GPIO_readPins(GREY_6_PORT, GREY_6_PIN_9_PIN) == 0) val |= 0x04;
    if (DL_GPIO_readPins(GREY_7_PORT, GREY_7_PIN_10_PIN) == 0) val |= 0x02;
    if (DL_GPIO_readPins(GREY_8_PORT, GREY_8_PIN_11_PIN) == 0) val |= 0x01;
    return val;
}

int16_t Grayscale_GetError(void) {
    uint8_t raw = Grayscale_Read();
    int16_t error = 0, sum_weight = 0;

    // 加权平均计算偏差
    for (int i = 0; i < 8; i++) {
        if ((raw >> (7 - i)) & 0x01) {  // 检测到黑线
            error += weight[i];
            sum_weight++;
        }
    }

    if (sum_weight == 0) {
        static int16_t last_error = 0;
        return last_error;   // 没有检测到黑线，保持上次偏差
    }

    error = error * 100 / sum_weight;   // 归一化到 -100~100
    if (error > 100) error = 100;
    if (error < -100) error = -100;
    return error;
}

void Line_Follow(uint16_t base_speed, float kp) {
    int16_t err = Grayscale_GetError();
    int16_t correction = (int16_t)(kp * err);
    /* Motor_SetDifferential 内部已处理限幅 (-1000~1000) 和方向 GPIO */
    Motor_SetDifferential((int16_t)base_speed - correction,
                          (int16_t)base_speed + correction);
}