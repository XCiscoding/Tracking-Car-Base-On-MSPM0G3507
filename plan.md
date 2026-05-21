# 自动行驶小车 H 题 —— 项目实施计划

最后更新：2026-05-20

---

## 赛题摘要

2024 全国大学生电子设计竞赛 H 题【本科组/高职高专组】

场地：220cm×120cm，两个对称半圆弧（r=40cm，黑色线宽1.8cm），四顶点 A/B/C/D。

```
   A ←100cm→ B
  ↗            ↘
(左弧 r=40)  (右弧 r=40)
  ↘            ↗
   D ←100cm→ C
```

**直线段**（A-B 顶部，D-C 底部）：无黑线，靠陀螺仪+编码器控制。
**弧线段**（A-D 左弧，B-C 右弧）：有黑线，靠灰度传感器巡线。

### 任务与得分

| 任务 | 路径 | 时限 | 分值 |
|------|------|------|------|
| (1) | A→B，到B停车+声光提示 | 15s | 20分 |
| (2) | A→B→弧→C→D→弧→A，每点声光提示 | 30s | 20分 |
| (3) | A→弧→D→C→弧→B→D→弧→A，每点声光提示 | 40s | 30分 |
| (4) | 按任务(3)路径跑4圈，用时越少越好 | — | 30分 |
| 设计报告 | — | — | 20分 |

> 注：题目规定小车只能前进，不得后退，不得使用摄像头。

---

## 硬件配置

### MCU & 开发板

```
MCU:      MSPM0G3507
Board:    LP-MSPM0G3507（板载 XDS110，走 CMSIS-DAP）
IDE:      Keil MDK 5.39 at D:\Keil5
Compiler: ARMCLANG V6.21
SDK:      C:\ti\mspm0_sdk_2_01_00_03
DFP:      TexasInstruments.MSPM0G1X0X_G3X0X_DFP 1.3.1（已打 FLM 补丁）
```

### 外设与引脚分配（最终定稿）

| 外设 | 引脚 | 外设实例 | 备注 |
|------|------|----------|------|
| OLED SDA | PA0 (PINCM1) | I2C0 | 已完成 |
| OLED SCL | PA1 (PINCM2) | I2C0 | 已完成 |
| LED | PA7 (PINCM14) | GPIO | 已完成 |
| 左轮 PWM | PA8 (PINCM19) | TIMA0 CC0 | 已完成 |
| 左轮 AIN1 | PA13 (PINCM35) | GPIO | 已完成 |
| 左轮 AIN2 | PA14 (PINCM36) | GPIO | 已完成 |
| 右轮 PWM | PA24 (PINCM54) | TIMA1 CC1 | 已完成 |
| 右轮 BIN1 | PA16 (PINCM38) | GPIO | 已完成 |
| 右轮 BIN2 | PA17 (PINCM39) | GPIO | 已完成 |
| **IMU UART RX** | **PB6** | **UART1** | 待实现 |
| **IMU UART TX** | **PB7** | **UART1** | 待实现 |
| **左编码器 A相** | **PA12** | **TIMG0 CCP0** | ✅ 已验证（CC0_DN_EVENT，DOWN计数器） |
| **右编码器 A相** | **PB16** | **TIMG8 CCP1** | ✅ 已配置，待实物验证 |
| **蜂鸣器** | **PB17** | **GPIO** | 低电平触发有源，待实现 |
| **灰度传感器** | **PB2/PB3/PA27/PB20/PB24** | ADC/GPIO | 数量待确认，待实现 |

> 空闲备用：PB24（灰度传感器备用）

### 关键外设规格

- **ATK-IMU901（六轴版）**：UART 串口输出欧拉角，默认波特率 115200，100Hz 输出
  - 六轴（加速度计+陀螺仪），无磁力计，偏航角靠积分，会漂移
  - 漂移解决方案：每次从弧线段进入直线段时调 `IMU_ResetYaw()` 清零
- **有源蜂鸣器（低电平触发）**：GPIO 拉低发声
- **编码器**：只接 A 相（单路脉冲计数），小车只前进不后退，无需方向判断

---

## 软件架构

### 模块结构

```
keil/
├── user_imu/
│   ├── imu.c      UART1 中断接收 + ATK-IMU901 帧解析 + 偏航角积分
│   └── imu.h      IMU_Init() / IMU_GetYaw() / IMU_ResetYaw()
├── user_buzzer/
│   ├── buzzer.c   Buzzer_Init() / Buzzer_Beep(n) / Buzzer_BeepMs(ms)
│   └── buzzer.h
├── user_motor/
│   ├── motor.c    PWM控制 + 方向GPIO + 编码器A相 + SysTick + delay_ms
│   └── motor.h    Motor_Init/Stop/Forward/SetDifferential/GetLeftDuty/GetRightDuty
│                  Encoder_Reset/GetPulse/GetDistanceMM
├── user_grayscale/
│   ├── grayscale.c  读传感器 + 判线偏移量 + Line_Follow()
│   └── grayscale.h  Grayscale_Init() / Grayscale_GetPos() / Line_Follow()
├── user_oled/
│   ├── oled.c     SSD1306 驱动（已完成）
│   ├── oled.h
│   ├── i2c_hal.c
│   ├── font_5x7.c
│   └── ...
├── led.h          LED_ON / LED_OFF 两个宏（无需 .c，直接操作 PA7）
└── empty.c        主状态机（只含高层逻辑，无硬件细节）
```

### 状态机设计

```c
// 系统顶层状态
typedef enum {
    STATE_INIT,         // 初始化、IMU校准、等待启动
    STATE_IDLE,         // 等待按键选题（Task 1/2/3/4）
    STATE_RUN_STRAIGHT, // 直线段：陀螺仪航向闭环 + 编码器里程停车
    STATE_RUN_ARC,      // 弧线段：灰度传感器巡线
    STATE_WAYPOINT,     // 到达 ABCD 点：停车→声光提示→进下一段
    STATE_DONE,         // 全部完成，停车
} SysState;
```

### 路线表设计（段序列执行器）

```c
typedef enum { SEG_STRAIGHT, SEG_ARC } SegType;

typedef struct {
    SegType  type;
    uint32_t dist_mm;  // 仅 STRAIGHT 用（弧线靠传感器判终点）
    uint8_t  beep_n;   // 到达终点时声光提示次数
} RouteSegment;

// Task 1: A→B
static const RouteSegment ROUTE_TASK1[] = {
    { SEG_STRAIGHT, 1000, 1 },
};

// Task 2: A→B→(弧)→C→D→(弧)→A 顺时针
static const RouteSegment ROUTE_TASK2[] = {
    { SEG_STRAIGHT, 1000, 1 },  // A→B
    { SEG_ARC,      0,    1 },  // B→C（右弧）
    { SEG_STRAIGHT, 1000, 1 },  // C→D
    { SEG_ARC,      0,    1 },  // D→A（左弧）
};

// Task 3/4 类似，路径方向不同
```

### 直线段控制逻辑

```
IMU_ResetYaw()          ← 进入直线时清零，消除弧线段积累的漂移

while (dist < target) {
    yaw = IMU_GetYaw()
    err = yaw - 0
    Motor_SetDifferential(base - Kp*err, base + Kp*err)
    dist = Encoder_GetDistanceMM()
}
Motor_Stop()
```

### 直线段停车策略（已决策 2026-05-21）

**分阶段实现**：

- **阶段一（当前）：方案 A — 纯编码器里程停车**
  - 走完 1000mm 即停，简单可验证
  - 不依赖灰度传感器，现在就能跑

- **阶段二（灰度完成后）：方案 C — 编码器 + 灰度双重触发**
  - 满足任一条件停车：
    1. 编码器 ≥ 900mm **且** 灰度检测到黑线 → 精确停在 B/D 点
    2. 编码器 ≥ 1100mm 超时保底 → 防灰度漏检
  - 用物理地标（黑线）修正里程误差，停车位置更准

### 弧线段控制逻辑

```
Line_Follow()           ← 每次主循环调用
// 内部：读灰度 → 计算偏差 → Motor_SetDifferential(left, right)
// 终点判断：编码器累积距离 或 特定传感器图案（到达顶点时所有传感器离线）
```

---

## 当前进度

### 已完成 ✅

- Keil 工程搭建（FLM补丁、RAM地址、编译0错误）
- OLED 驱动（SSD1306/I2C0/PA0-PA1，先清后写规则）
- 左轮 PWM（TIMA0/PA8）+ 方向控制（PA13/PA14）
- 右轮 PWM（TIMA1/PA24）+ 方向控制（PA16/PA17）
- 双轮前进验证：`Motor_SetDifferential(500, 500)` 实测通过
- `Motor_Forward/Stop/Turn_Left/Turn_Right/SetDifferential` 全部实现
- OLED 显示电机状态 `L:xxxx R:xxxx / RUNNING/STOP`
- 蜂鸣器引脚选定（PB17），架构设计完成
- **左右编码器配置完成（2026-05-21）**：
  - 左编码器：PA12 → TIMG0 CCP0，CC0_DN_EVENT ✅ 实测脉冲累积正常
  - 右编码器：PB16 → TIMG8 CCP1，CC1_DN_EVENT ✅ 待实物验证
  - 关键：`EDGE_TIME` = DOWN 计数器 → 必须用 `CCx_DN_EVENT`（不是 UP_EVENT）
  - API：`Encoder_GetLeftPulse()` / `Encoder_GetRightPulse()` / `Motor_GetTickMs()`
  - 编码器接线注意：VCC/GND 反接会导致信号线恒定高电平，A 相无变化

### 下一步（按顺序）

1. ✅ **验证左右编码器**：实测 PPR≈270，WHEEL_CIRCUM=188.5mm，已写入 motor.c

2. **调通编码器距离控制（方案 A）**
   - 修复 ISR 假脉冲问题（已加状态检查，待验证）
   - `Motor_GoDistance(50, 300)` 实测走 50mm 停车
   - 若仍有 EMI 假脉冲，在 PA12/PB16 信号线对 GND 并联 100nF 滤波电容

3. **`user_buzzer/buzzer.c/h`**
   - PB17 GPIO 初始化（输出，默认高电平=不响）
   - `Buzzer_Init() / Buzzer_Beep(n)`：响 n 次，每次 100ms

4. **`user_imu/imu.c/h`**
   - UART1 初始化（PB6=RX, PB7=TX, 115200）
   - UART1 RX 中断接收 ATK-IMU901 数据帧
   - 解析偏航角
   - 导出 `IMU_Init() / IMU_GetYaw() / IMU_ResetYaw()`

5. **`empty.c` Task 1 直线走通（方案 A）**
   - 陀螺仪航向闭环 + 编码器 1000mm 停车
   - 到 B 点：停车 → LED 亮 → 蜂鸣器响 1 次

6. **灰度传感器模块**（数量待确认）
   - 引脚从 PB2/PB3/PA27/PB20/PB24 选
   - `Grayscale_Init() / Grayscale_GetPos() / Line_Follow()`
   - 叠加到直线段停车逻辑 → 升级为方案 C

7. **`empty.c` 状态机重写（Task 2/3/4）**
   - 段序列执行器
   - 直线/弧线状态切换

---

## 不要踩的旧坑

```
不要把 RAM for Algorithm 改回 0x20000000
不要重新启用 SysConfig Before Build（会覆盖 ti_msp_dl_config.c/h）
不要用补空格方式覆盖 OLED 旧字符（|= 机制，空格无效）
不要把 OLED I2C 配置到 I2C1 或 PA0/PA1 之外的引脚
编码器用 EDGE_TIME（DOWN计数器）时，中断事件必须用 CCx_DN_EVENT（不能用 CCx_UP_EVENT）
推挽输出编码器不能用导线直接短接GND测试（会通过输出晶体管短路 → 系统重置）
编码器接线前用万用表确认 VCC/GND 极性：恒高 = VCC，恒低 = GND，切换 = A相
不要用 TI XDS Debugger（32/64位DLL不兼容），用 CMSIS-DAP
不要在 Keil 根目录留同名空文件（会遮蔽 user_xxx 子目录的正确头文件）
不要在 Keil 弹出"保存 X.h?"时随意点确定（会用编辑器空内容覆盖磁盘文件）
不要把右轮 PA24 接线遗漏（J4接口，不在常用的 J1/J2 区域）
```

---

## 关键数据备忘

```
OLED 地址:         0x3C（7-bit）
左轮 PWM 周期:     1000（32MHz/1000 = 32kHz）
右轮 PWM 周期:     1000
MIN_START_PWM:     80（低于此值电机不启动）
SysTick 频率:      1ms（SysTick_Config(32000)）
A→B 距离:         1000mm（100cm）
C→D 距离:         1000mm（100cm）
弧长（半圆r=40）:  约 1257mm（π×40×10）
IMU UART 波特率:   115200
IMU 输出频率:      100Hz（10ms/帧）
```
