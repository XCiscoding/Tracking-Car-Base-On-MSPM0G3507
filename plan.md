# 自动行驶小车 H 题 —— 项目实施计划

最后更新：2026-05-23

---

## 赛题摘要

2024 全国大学生电子设计竞赛 H 题【本科组/高职高专组】

场地：220cm×120cm，两个对称半圆弧（r=40cm，黑色线宽1.8cm），四顶点 A/B/C/D。

```text
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

```text
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
| IMU MCU TX→IMU RX | PB6 (PINCM23) | UART1 TX | 已配置 |
| IMU MCU RX←IMU TX | PB7 (PINCM24) | UART1 RX | 已确认接收和解析通 |
| 左编码器 A相 | PA12 | TIMG0 CCP0 | 已验证（CC0_DN_EVENT，DOWN计数器） |
| 右编码器 A相 | PB16 | TIMG8 CCP1 | 已验证 |
| 蜂鸣器 | PB17 | GPIO | 低电平触发有源，待实现 |
| 灰度传感器 | PB2/PB3/PA27/PB20/PB24 | ADC/GPIO | 数量待确认，待实现 |

> 空闲备用：PB24（灰度传感器备用）

### 关键外设规格

- **正点原子 ATK-IMU901（六轴串口版）**：当前按 UART1、115200、8N1 接入。
  - 已确认 PB7 有持续原始字节进入。
  - 已证伪 `AA FF` ATK 假设帧。
  - 当前实测真实帧：`55 55 CMD LEN DATA SUM`。
  - `OK` 持续增长，`CMD` 在 `0x01/0x02/0x03/0x06` 间切换。
  - 传统 `CMD=0x06` 的 R/P/Y 水平转动不可信；当前使用 `D0 = CMD=0x02[0] - bias` 作为航向候选。
- **有源蜂鸣器（低电平触发）**：GPIO 拉低发声。
- **编码器**：只接 A 相（单路脉冲计数），小车只前进不后退，无需方向判断。

---

## 软件架构

### 模块结构

```text
keil/
├── user_imu/
│   ├── imu.c      UART1 中断接收 + 55 55 帧解析 + D0 航向候选
│   └── imu.h      IMU_Init() / IMU_GetHeadingGyroRaw() / 诊断接口
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
└── empty.c        当前为 D0 航向修正 500mm 直线测试入口
```

### 状态机设计（后续恢复）

```c
typedef enum {
    STATE_INIT,
    STATE_IDLE,
    STATE_RUN_STRAIGHT,
    STATE_RUN_ARC,
    STATE_WAYPOINT,
    STATE_DONE,
} SysState;
```

### 直线段控制逻辑（当前正在调试）

```text
第一层：编码器左右轮同步，让 L-R 接近 0
第二层：D0 航向修正，让车头偏角接近 0
停车：编码器距离达到目标后停，后续灰度完成后再做黑线辅助停车
```

当前正在 500mm 级别验证，不要直接恢复 1000mm Task 1。

---

## 当前进度

### 已完成

- Keil 工程搭建（FLM补丁、RAM地址、编译0错误）。
- OLED 驱动（SSD1306/I2C0/PA0-PA1，先清后写规则）。
- 左轮 PWM（TIMA0/PA8）+ 方向控制（PA13/PA14）。
- 右轮 PWM（TIMA1/PA24）+ 方向控制（PA16/PA17）。
- 双轮前进验证：`Motor_SetDifferential(500, 500)` 实测通过。
- `Motor_Forward/Stop/Turn_Left/Turn_Right/SetDifferential` 全部实现。
- OLED 显示电机状态与调试数据。
- 蜂鸣器引脚选定（PB17），架构设计完成。
- 左右编码器配置完成：
  - 左编码器：PA12 → TIMG0 CCP0，CC0_DN_EVENT，实测脉冲累积正常。
  - 右编码器：PB16 → TIMG8 CCP1，CC1_DN_EVENT，当前能输出右轮脉冲。
  - 关键：`EDGE_TIME` = DOWN 计数器 → 必须用 `CCx_DN_EVENT`（不是 UP_EVENT）。
  - API：`Encoder_GetLeftPulse()` / `Encoder_GetRightPulse()` / `Motor_GetTickMs()`。
- OLED_Refresh 大包优化：I2C 事务数 1048 → 32，100ms 刷新不再过载。
- 直线实验记录建立：`experiment_log.md` 记录参数、OLED D、左右脉冲、实测距离、偏角和结论。
- 编码器第一层闭环已基本完成：最终 `E=L-R` 可压到约 -1~7，`C` 多数在 -2~14。
- IMU UART 接收链路通，真实协议已解析通。
- D0 航向修正已经接入 500mm 直线测试；当前 4 次中 3 次接近直线，但仍偶发右偏。

### 当前阶段：D0 航向直线调试

当前不是继续找 IMU 协议，也不是恢复完整路线状态机，而是解决 500mm D0 直线测试的起步稳定性。

当前固件：`empty.c` D0 航向修正直线测试，会启动电机跑 500mm。

当前 OLED 显示：

```text
D0: 航向候选误差
E : 左右编码器误差 L-R
EC: 编码器修正量
HC: D0 航向修正量
D : 编码器距离 mm
L/R: 左右轮脉冲
```

当前最新测试：

```text
几乎没有偏移，D0=9，E=6，HC=0
往右偏45°，D0=119，E=4，HC=2
几乎没有偏移，D0=44，E=4，HC=1
往右偏10°，D0=14，E=4，HC=0
```

判断：

- D0 修正方向已基本正确。
- 偶发大偏那次停车时 `D0/E/HC` 都不大，说明偏差可能发生在起步瞬间。
- 下一步不应继续盲目加 D0 增益，而应改启动段控制。

### 已证伪的 IMU 路线

- 不能继续把传统 Yaw 当航向：手动水平转 30° 时只变化约 1°。
- 不能继续期待 WIT `CMD=0x52` 陀螺仪帧：实测未出现。
- 不能把 `CMD=0x01/0x03` 当前候选通道当航向反馈：水平转动响应不合适。
- 不能只凭「ATK-IMU901」这个模块名就认定当前输出一定是 `AA FF` 帧：实测是 `55 55`。

---

## 下一步（按顺序）

1. **修改启动段控制**
   - 当前启动段是 EC 和 HC 都关，只按固定 PWM 左 170 / 右 220。
   - 建议改成：
     ```text
     0~250ms：启用编码器 EC，禁用 D0 的 HC
     250ms 后：EC + HC 都启用
     ```
   - 理由：起步时 IMU 可能抖，先不让 HC 乱修；但编码器同步必须尽早开，防止左右轮起步脉冲拉开。

2. **加快 OLED 刷新**
   - 把 `IMU_DIAG_REFRESH_MS` 从 500ms 改到 200ms。
   - 目的：观察起步瞬间 `D0/E/EC/HC` 是否跳变。

3. **重新做 4 次 500mm 测试**
   - 记录：最终偏角、停车时 `D0/E/EC/HC`。
   - 重点看是否还出现右偏 45°/60° 这种偶发大偏。

4. **若仍偶发大偏**
   - 不要先加 D0 增益。
   - 优先检查：地面摩擦、轮胎打滑、万向轮方向、重心、左右轮启动抓地差异。

5. **直线稳定后再推 Task 1**
   - 500mm 直线稳定后，再扩展到 1000mm。
   - 然后再加 B 点停车声光提示。

6. **`user_buzzer/buzzer.c/h`**
   - PB17 GPIO 初始化（输出，默认高电平=不响）。
   - `Buzzer_Init() / Buzzer_Beep(n)`：响 n 次，每次 100ms。

7. **灰度传感器模块**
   - 当前 `grayscale.c` 被临时移出编译列表，因为 GREY 宏未定义会报 17 个错。
   - 恢复前必须补齐引脚宏、GPIO 初始化和 `Grayscale_Read()`。
   - 后续用于弧线巡线与直线末端黑线停车。

---

## 协作与日志规则

- 用户口令「总结这次对话。」时，自动更新 `PROJECT_MEMORY.md`、`LONG_TERM_MEMORY.md`、`plan.md`、`ROLLING.md`、`experiment_log.md`、`debug_log.md` 和长期 telos 记忆。
- `experiment_log.md` 记录每轮测试参数、测试数据、现象和结论。
- `debug_log.md` 记录每次报错、异常现象、判断、修复和验证。

---

## 不要踩的旧坑

```text
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
不要把传统 Yaw/Roll/Pitch 当当前航向真相源
不要继续把 ATK-IMU901 按 AA FF 帧解析，当前实测是 55 55
不要把偶发大偏直接归咎于 D0 增益，先查启动段
```

---

## 关键数据备忘

```text
OLED 地址:         0x3C（7-bit）
左轮 PWM 周期:     1000（32MHz/1000 = 32kHz）
右轮 PWM 周期:     1000
MIN_START_PWM:     80（低于此值电机不启动）
SysTick 频率:      1ms（SysTick_Config(32000)）
A→B 距离:         1000mm（100cm）
C→D 距离:         1000mm（100cm）
弧长（半圆r=40）:  约 1257mm（π×40×10）
IMU UART 波特率:   115200
IMU 当前协议:      55 55 CMD LEN DATA SUM
当前航向候选:      D0 = CMD=0x02[0] - bias
当前直线目标:      500mm
当前基础 PWM:      左 170，右 220
当前 D0 修正:      HC = D0 / 40，最大 ±50
```
