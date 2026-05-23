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
| 灰度 X1 | PB2 | GPIO | 亚博八路巡线模块，已接入诊断 |
| 灰度 X2 | PB3 | GPIO | 亚博八路巡线模块，已接入诊断 |
| 灰度 X3 | 不接 | — | 空 |
| 灰度 X4 | PA27 | GPIO | 亚博八路巡线模块，已接入诊断 |
| 灰度 X5 | PB20 | GPIO | 亚博八路巡线模块，已接入诊断 |
| 灰度 X6 | 不接 | — | 空 |
| 灰度 X7 | PB14 | GPIO | 亚博八路巡线模块，已接入诊断 |
| 灰度 X8 | PB15 | GPIO | 亚博八路巡线模块，已接入诊断 |

> 灰度模块为外部 5V 供电，必须和 MSPM0 共地；若输出高电平为 5V，建议加分压/电平转换。模块有效检测距离约 3~4mm，悬空测试不可靠。

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
│   ├── grayscale.c  亚博八路巡线模块 6 路读取 + 灰度偏差 + Line_Follow()
│   └── grayscale.h  Grayscale_Init() / Grayscale_Read() / Grayscale_GetError() / Line_Follow()
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
第一层：编码器左右轮同步，让 L-R 接近 0（当前加大到 KP=4、MAX=120）
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
- 亚博八路巡线模块已开始移植：当前只用 X1/X2/X4/X5/X7/X8 六路，X3/X6 不接；`grayscale.c` 已加回 Keil 编译列表并能进入灰度诊断页。

### 当前阶段：灰度静态诊断

当前固件已从 D0 直线测试切到 `Run_Grayscale_Diag()`，烧录后不会启动电机，只显示亚博八路巡线模块的 6 路状态。

当前接线：

```text
X1=PB2, X2=PB3, X3=不接, X4=PA27, X5=PB20, X6=不接, X7=PB14, X8=PB15
```

当前 OLED 显示：

```text
G: X1/X2/X4/X5/X7/X8 六路状态
E: 灰度偏差
N: 触发路数
```

用户现场描述：

```text
检测到黑线时模块灯亮，G 的对应位变 0，N+1；模块有效检测距离约 3~4mm。
```

当前代码仍需实测复核：`Grayscale_Read()` 目前把低电平置为 bit=1，`N` 和 `E` 都按 bit=1 处理。下一轮必须先确认 OLED 的 G/N/E 与真实模块灯状态是否一致，不能直接把灰度接进第一题停车逻辑。

注意：模块要求 5V，用户使用外部 5V 供电。必须共地；若输出高电平为 5V，MSPM0 GPIO 有风险。用户当前选择继续调代码，但读数异常时应回到电平检查。

### 已暂停阶段：D0 航向直线调试

当前不是继续找 IMU 协议，也不是恢复完整路线状态机。D0 直线控制参数保留，但先暂停，等灰度诊断完成后再决定如何接入 Task 1。

当前 OLED 显示：

```text
D0: 航向候选误差
E : 左右编码器误差 L-R
EC: 编码器修正量
HC: D0 航向修正量
M : 控制阶段，0=软启动，1=正常闭环
D : 编码器距离 mm
LP/RP: 左右轮实际 PWM
```

当前最新测试：

```text
左偏30°，D0=0，E=-3，EC=-12，HC=0，LP=212，RP=188，M=1，D=440
左偏60°，D0=-7082，E=-2，EC=-6，HC=0，LP=206，RP=194，M=1，D=440
左偏15°，D0=-17021，E=-4，EC=-12，HC=0，LP=212，RP=188，M=1，D=438
左偏30°，D0=0，E=-1，EC=-3，HC=0，LP=203，RP=197，M=1，D=438
左偏10°，D0=0，E=0，EC=0，HC=0，LP=200，RP=200，M=1，D=441
左偏30°，D0=71，E=-15，EC=0，HC=0，LP=215，RP=105，M=1，D=440
```

判断：

- 取消固定补偿后，小车表现为稳定左偏。
- `HC` 基本一直为 0，说明上一版 D0 修正没有实际参与。
- `E` 大多很小，说明编码器动态同步能压住左右脉冲差，但不能直接保证车头方向。
- 最后一组 `EC=0` 但 `LP/RP=215/105` 与公式不一致，下一轮要重点复查记录顺序或 OLED 刷新时序。
- 用户已把当前代码进一步改为 `D0_DIV=20`、`D0_MAX_CORR=80`，且 HC 公式为 `-(D0-target_d0)/20`，用于让 D0 P 修正真正介入。

### 已证伪/待复核的 IMU 路线

- 不能继续把传统 Yaw 当航向：手动水平转 30° 时只变化约 1°。
- 不能继续期待 WIT `CMD=0x52` 陀螺仪帧：实测未出现。
- 不能把 `CMD=0x01/0x03` 当前候选通道当航向反馈：水平转动响应不合适。
- 不能只凭「ATK-IMU901」这个模块名就认定当前输出一定是 `AA FF` 帧：实测是 `55 55`。
- 不能因为 ATK-MS901M/IMU901 自带姿态解析，就直接把 R/P/Y 当小车车头航向；官方姿态角比例是 `raw / 32768 * 180`，但比例修正只能解决量纲问题，不能替代模块安装姿态、坐标轴和磁力计环境下的轴映射验证。
- 若要重测 IMU，可行性高，但必须先做静态轴映射：正中、水平右转 30°、回正、水平左转 30°、回正、抬头、压头、侧倾。只有满足右转/左转反号且回正接近初始值的通道，才能接 PID。

---

## 下一步（按顺序）

1. **先完成灰度静态诊断**
   - 固定模块离地约 3~4mm，不要悬空测。
   - 记录白底、单路黑线、多路黑线时的 `G/E/N`。
   - 重点确认：黑线灯亮时，OLED 对应位到底是 0 还是 1，`N` 是否真实 +1。
   - 理由：当前用户描述和代码内部 bit 语义仍可能冲突，未确认前接入第一题会把停车/巡线判断做反。

2. **确认 5V 灰度模块的电气安全**
   - 外部 5V 供电必须和 MSPM0 共地。
   - 若输出高电平接近 5V，建议加分压/电平转换后再长期使用。
   - 理由：代码能修逻辑，不能解决 5V 直灌 GPIO 的硬件风险。

3. **灰度通过后再接入 Task 1**
   - 先恢复编码器 + D0 直线控制。
   - 跑 A→B 1000mm 时，灰度只作为接近 B 点后的黑线/终点辅助停车，不要全程参与控制。
   - 建议策略：编码器距离接近 850mm 后，若灰度触发路数达到阈值，再停车并声光提示；编码器 1000mm 保底停车。

4. **如需复测 IMU，先做静态轴映射页**
   - 同屏显示 `CMD=0x06` 的 R/P/Y、`CMD=0x02` 的 D0/D1/D2，最好再带 `CMD=0x01` 三轴。
   - 动作顺序：正中静止、水平右转约 30°、回正、水平左转约 30°、回正、抬车头、压车头、左右侧倾。
   - `CMD=0x06` 官方姿态角复核时按说明书比例 `raw / 32768 * 180`。
   - 只有满足右转明显变一边、回正接近初始值、左转明显变另一边且抬头/侧倾不大幅混入的通道，才允许接入直线 PID。

5. **直线稳定后再加蜂鸣器/LED 提示**
   - PB17 低电平有源蜂鸣器：到 B 后响若干次。
   - PA7 LED 同步提示。

6. **暂时不要恢复完整路线状态机和弧线巡线**
   - 第一题只需要 A→B 直线停车。
   - 弧线灰度巡线等第一题稳定后再做。

7. **保留 D0 直线调试结论**
   - 若恢复 D0 直线控制，继续使用已记录的软启动 + EC + HC 思路：车动前锁定 target_d0，起步 150ms 禁用 HC，之后再启用 `HC=-(D0-target_d0)/20` 并过滤大跳变。

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
当前基础 PWM:      200，软启动 170
当前 EC 修正:       KP=4，MAX=120
当前 D0 修正:      HC = -(D0-target_d0) / 20，最大 ±80，abs(error)>3000 时 HC=0
```
