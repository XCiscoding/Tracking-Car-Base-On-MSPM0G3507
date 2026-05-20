#ifndef _MOTOR_H_
#define _MOTOR_H_

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 电机模块初始化
 * - 初始化 SysTick 提供毫秒计时
 * - 停止电机、复位编码器
 */
void Motor_Init(void);

/**
 * @brief 状态机刷新函数 (必须在主循环中周期调用，建议 1~10ms)
 * 用于处理非阻塞前进、超时检测、堵转检测。
 */
void Motor_Tick(void);

// ---------- 基础运动 ----------
void Motor_Forward(uint16_t speed);      // 前进，速度 0~1000
void Motor_Stop(void);                   // 停车
void Motor_Turn_Left(uint16_t speed);    // 原地左转 (右轮转，左轮停)
void Motor_Turn_Right(uint16_t speed);   // 原地右转

// ---------- 差速控制 (供 motion 层调用) ----------
// left/right 范围 -1000~1000，负值反转
void Motor_SetDifferential(int16_t left, int16_t right);

// ---------- 距离控制 ----------
void Motor_GoDistance(uint32_t distance_mm, uint16_t speed);          // 阻塞式前进指定距离
void Motor_GoDistanceAsync(uint32_t distance_mm, uint16_t speed, uint32_t timeout_ms); // 非阻塞启动
bool Motor_IsMoving(void);                                            // 查询是否在运动中

// ---------- 编码器 ----------
void    Encoder_Reset(void);            // 清零编码器计数值
int32_t Encoder_GetPulse(void);         // 获取有符号脉冲数 (前进为正)
float   Encoder_GetDistanceMM(void);    // 获取累计距离 (毫米)

// ---------- 故障检测 ----------
bool Encoder_IsStuck(void);             // 检测是否堵转 (电机转动但编码器无变化)

// ---------- 延时工具 ----------
void delay_ms(uint32_t ms);             // 基于 SysTick，需在 Motor_Init() 后使用

// ---------- 状态查询 ----------
uint16_t Motor_GetLeftDuty(void);       // 当前左轮实际占空比 (0~1000，含限幅)
uint16_t Motor_GetRightDuty(void);      // 当前右轮实际占空比

#endif
