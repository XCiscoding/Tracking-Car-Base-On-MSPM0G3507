#include "motor.h"
#include "ti_msp_dl_config.h"
#include <ti/driverlib/dl_timerg.h>

// ========== 用户标定参数 ==========
#define WHEEL_CIRCUM_MM        210.0f   // 轮子周长 (mm)
#define ENCODER_PPR            400      // 编码器每转脉冲数
#define DIST_PER_PULSE         (WHEEL_CIRCUM_MM / ENCODER_PPR)  // 每个脉冲对应的毫米数

// ========== 电机参数 ==========
#define MIN_START_PWM          80       // 电机启动最小 PWM (0~1000)
#define MAX_PWM                1000
#define STUCK_TIMEOUT_MS       500      // 堵转检测时间窗口 (ms)

// ========== 状态机 ==========
typedef enum {
    STATE_IDLE,
    STATE_MOVING_ASYNC
} MotorState;

static MotorState motor_state = STATE_IDLE;
static uint32_t target_pulse = 0;       // 目标脉冲数
static uint32_t start_tick = 0;         // 开始时刻 (ms)
static uint32_t timeout_ms = 0;         // 超时时间
static int32_t last_pulse = 0;          // 上次脉冲数 (用于堵转检测)

// ---------- 电机当前占空比（供 OLED 状态显示用） ----------
static uint16_t s_left_duty  = 0;
static uint16_t s_right_duty = 0;

// ---------- 内部函数声明 ----------
static void Motor_SetLeftPWM(uint16_t duty);
static void Motor_SetRightPWM(uint16_t duty);
static uint32_t get_tick_ms(void);
static void Motor_InitTick(void);

// ---------- PWM 限幅与最小启动值 ----------
static void Motor_SetLeftPWM(uint16_t duty) {
    if (duty > 0 && duty < MIN_START_PWM) duty = MIN_START_PWM;
    if (duty > MAX_PWM) duty = MAX_PWM;
    s_left_duty = duty;
    DL_TimerA_setCaptureCompareValue(PWM_LEFT_INST, duty, DL_TIMER_CC_0_INDEX);
}

static void Motor_SetRightPWM(uint16_t duty) {
    if (duty > 0 && duty < MIN_START_PWM) duty = MIN_START_PWM;
    if (duty > MAX_PWM) duty = MAX_PWM;
    s_right_duty = duty;
    DL_TimerA_setCaptureCompareValue(PWM_RIGHT_INST, duty, DL_TIMER_CC_1_INDEX);  /* PA24=TIMA1_CCP1 */
}

/* 差速接口：供 motion 层调用，left/right 范围 -1000~1000，负值反转 */
void Motor_SetDifferential(int16_t left, int16_t right)
{
    /* 左轮方向 */
    if (left >= 0) {
        DL_GPIO_setPins(MOTOR_AIN1_PORT,  MOTOR_AIN1_PIN_0_PIN);
        DL_GPIO_clearPins(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN_1_PIN);
    } else {
        DL_GPIO_clearPins(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN_0_PIN);
        DL_GPIO_setPins(MOTOR_AIN2_PORT,   MOTOR_AIN2_PIN_1_PIN);
        left = -left;
    }
    /* 右轮方向 */
    if (right >= 0) {
        DL_GPIO_setPins(MOTOR_BIN1_PORT,  MOTOR_BIN1_PIN_2_PIN);
        DL_GPIO_clearPins(MOTOR_BIN2_PORT, MOTOR_BIN2_PIN_3_PIN);
    } else {
        DL_GPIO_clearPins(MOTOR_BIN1_PORT, MOTOR_BIN1_PIN_2_PIN);
        DL_GPIO_setPins(MOTOR_BIN2_PORT,   MOTOR_BIN2_PIN_3_PIN);
        right = -right;
    }
    Motor_SetLeftPWM((uint16_t)left);
    Motor_SetRightPWM((uint16_t)right);
}

// ---------- 毫秒计时 (SysTick) ----------
static volatile uint32_t g_sysTickCount = 0;
void SysTick_Handler(void) {
    g_sysTickCount++;
}
static uint32_t get_tick_ms(void) {
    return g_sysTickCount;
}

void delay_ms(uint32_t ms) {
    uint32_t start = get_tick_ms();
    while ((get_tick_ms() - start) < ms) {}
}

static void Motor_InitTick(void) {
    // 假设 CPU 频率为 32MHz, 配置 SysTick 产生 1ms 中断
    SysTick_Config(32000);
}

// ---------- 编码器累计 (防回绕) ----------
static uint32_t encoder_last_raw = 0;
static int32_t encoder_accumulated = 0;
static void encoder_update(void) {
    uint32_t raw = DL_TimerG_getTimerCount(QEI_LEFT_INST);
    int32_t diff = (int32_t)(raw - encoder_last_raw);
    encoder_accumulated += diff;
    encoder_last_raw = raw;
}

void Encoder_Reset(void) {
    encoder_accumulated = 0;
    encoder_last_raw = DL_TimerG_getTimerCount(QEI_LEFT_INST);
    last_pulse = 0;
}

int32_t Encoder_GetPulse(void) {
    encoder_update();
    return encoder_accumulated;
}

float Encoder_GetDistanceMM(void) {
    return (float)Encoder_GetPulse() * DIST_PER_PULSE;
}

bool Encoder_IsStuck(void) {
    int32_t now = Encoder_GetPulse();
    bool stuck = (now == last_pulse);
    last_pulse = now;
    return stuck;
}

// ---------- 电机基础运动 ----------
void Motor_Init(void) {
    Motor_InitTick();   // 启动 SysTick（STBY 硬接 VCC，无需代码操作）
    Motor_Stop();
    Encoder_Reset();
    motor_state = STATE_IDLE;
}

void Motor_Forward(uint16_t speed) {
    motor_state = STATE_IDLE;   // 中断异步运动
    // 设置方向: 左电机正转
    DL_GPIO_setPins(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN_0_PIN);
    DL_GPIO_clearPins(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN_1_PIN);
    // 右电机正转
    DL_GPIO_setPins(MOTOR_BIN1_PORT, MOTOR_BIN1_PIN_2_PIN);
    DL_GPIO_clearPins(MOTOR_BIN2_PORT, MOTOR_BIN2_PIN_3_PIN);
    Motor_SetLeftPWM(speed);
    Motor_SetRightPWM(speed);
}

void Motor_Stop(void) {
    motor_state = STATE_IDLE;
    DL_GPIO_clearPins(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN_0_PIN | MOTOR_AIN2_PIN_1_PIN);
    DL_GPIO_clearPins(MOTOR_BIN1_PORT, MOTOR_BIN1_PIN_2_PIN | MOTOR_BIN2_PIN_3_PIN);
    Motor_SetLeftPWM(0);
    Motor_SetRightPWM(0);
}

void Motor_Turn_Left(uint16_t speed) {
    motor_state = STATE_IDLE;
    // 左轮停止
    DL_GPIO_clearPins(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN_0_PIN | MOTOR_AIN2_PIN_1_PIN);
    Motor_SetLeftPWM(0);
    // 右轮正转
    DL_GPIO_setPins(MOTOR_BIN1_PORT, MOTOR_BIN1_PIN_2_PIN);
    DL_GPIO_clearPins(MOTOR_BIN2_PORT, MOTOR_BIN2_PIN_3_PIN);
    Motor_SetRightPWM(speed);
}

void Motor_Turn_Right(uint16_t speed) {
    motor_state = STATE_IDLE;
    // 左轮正转
    DL_GPIO_setPins(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN_0_PIN);
    DL_GPIO_clearPins(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN_1_PIN);
    Motor_SetLeftPWM(speed);
    // 右轮停止
    DL_GPIO_clearPins(MOTOR_BIN1_PORT, MOTOR_BIN1_PIN_2_PIN | MOTOR_BIN2_PIN_3_PIN);
    Motor_SetRightPWM(0);
}

// ---------- 距离控制 ----------
void Motor_GoDistance(uint32_t distance_mm, uint16_t speed) {
    Encoder_Reset();
    Motor_Forward(speed);
    while (Encoder_GetDistanceMM() < distance_mm) {
        for (volatile uint32_t i = 0; i < 100; i++); // 微小延时，减少CPU占用
    }
    Motor_Stop();
}

void Motor_GoDistanceAsync(uint32_t distance_mm, uint16_t speed, uint32_t timeout_msec) {
    if (motor_state != STATE_IDLE) return;  // 运动中禁止重新启动
    Encoder_Reset();
    Motor_Forward(speed);
    target_pulse = (uint32_t)(distance_mm / DIST_PER_PULSE);
    start_tick = get_tick_ms();
    timeout_ms = timeout_msec;
    motor_state = STATE_MOVING_ASYNC;
    last_pulse = Encoder_GetPulse();
}

bool Motor_IsMoving(void) {
    return (motor_state != STATE_IDLE);
}

// ---------- 状态机 (必须在主循环中周期性调用) ----------
void Motor_Tick(void) {
    if (motor_state != STATE_MOVING_ASYNC) return;
    uint32_t now = get_tick_ms();

    encoder_update();
    int32_t cur = encoder_accumulated;

    // 到达目标脉冲 或 超时
    if (cur >= target_pulse || (now - start_tick) >= timeout_ms) {
        Motor_Stop();
        motor_state = STATE_IDLE;
        return;
    }

    // 堵转检测 (每200ms检查一次)
    static uint32_t last_check = 0;
    if (now - last_check >= 200) {
        if (cur == last_pulse) {
            Motor_Stop();
            motor_state = STATE_IDLE;
            return;
        }
        last_pulse = cur;
        last_check = now;
    }
}

// ---------- 状态查询 ----------
uint16_t Motor_GetLeftDuty(void)  { return s_left_duty;  }
uint16_t Motor_GetRightDuty(void) { return s_right_duty; }