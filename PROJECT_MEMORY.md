# 项目记忆：C:\ti\empty

最后更新：2026-05-23

长期记忆权威位置：

```text
C:\Users\26404\.claude\telos
```

本项目关键文件：

```text
Keil工程:      C:\ti\empty\keil\empty_LP_MSPM0G3507_nortos_keil.uvprojx
主程序真源:    C:\ti\empty\empty.c（工程里是 ../empty.c，不是 keil/empty.c）
电机/编码器:   C:\ti\empty\keil\user_motor\motor.c/h
IMU:           C:\ti\empty\keil\user_imu\imu.c/h
OLED:          C:\ti\empty\keil\user_oled\oled.c/h
实验记录:      C:\ti\empty\experiment_log.md
滚动交接:      C:\ti\empty\ROLLING.md
```

## 当前项目目标

实现 2024 全国大学生电子设计竞赛 H 题「自动行驶小车」。当前优先级仍是先跑通 Task 1：A→B 直线 1000mm，到 B 点停车并声光提示。

当前阶段是 **500mm 直线控制分层调试**：编码器第一层闭环已经基本可用，IMU 协议已解析通，当前使用 `D0 = CMD=0x02[0] - bias` 作为航向候选，正在解决起步阶段导致的偶发大偏差。

## 固定环境

```text
Workspace: C:\ti\empty
Keil:      D:\Keil5
Project:   C:\ti\empty\keil\empty_LP_MSPM0G3507_nortos_keil.uvprojx
SDK:       C:\ti\mspm0_sdk_2_01_00_03
DFP:       D:\Keil5\ARM\PACK\TexasInstruments\MSPM0G1X0X_G3X0X_DFP\1.3.1
Compiler:  ARMCLANG V6.21
Debugger:  CMSIS-DAP Debugger through onboard XDS110
```

## 烧录/构建硬约束

```text
Device: MSPM0G3507
RAM for Algorithm Start: 0x20200000
RAM for Algorithm Size:  0x1000
Flash Download: Erase Full Chip + Program + Verify + Reset and Run
Programming Algorithm: MSPM0G MAIN 128KB
Image alignment rule: Code + RO-data must be divisible by 8
```

若出现 `Programming Failed`，优先检查 `Code + RO-data` 是否 8 字节对齐、FLM 补丁是否还在、是否引入 printf/snprintf/locale。

## 当前工程状态

### `empty.c`

当前是 **D0 航向修正 500mm 直线测试模式**，烧录后会启动电机，不再是无电机 RAW 诊断页。

OLED 显示：

```text
D0: 航向候选误差，来自 CMD=0x02[0] 零偏后
E : 左右编码器误差 L-R
EC: 编码器修正量
HC: D0 航向修正量
D : 编码器距离 mm
L/R: 左右轮脉冲
```

当前直线测试参数：

```text
STRAIGHT_TARGET_MM      500.0f
STRAIGHT_BASE_PWM       220
STRAIGHT_LEFT_TRIM      -50
STRAIGHT_RIGHT_TRIM     0
STRAIGHT_ENC_KP         2
STRAIGHT_ENC_MAX_CORR   80
STRAIGHT_D0_DIV         40
STRAIGHT_D0_MAX_CORR    50
STRAIGHT_LOOP_MS        20
STRAIGHT_LAUNCH_MS      250
```

当前 D0 修正方向：

```text
left_pwm  = base + trim - EC - HC
right_pwm = base + trim + EC + HC
```

该方向比旧方向明显更对；不要轻易改回旧方向。

### `keil/user_imu/imu.c/h`

当前 IMU 真实协议已确认：

```text
[0x55][0x55][CMD][LEN][DATA...][SUM]
```

已知现象：

- `CMD` 在 `0x01/0x02/0x03/0x06` 间切换。
- `OK` 持续增长，checksum 解析已通。
- `CMD=0x06` 解析出的传统 `Roll/Pitch/Yaw` 水平转动车身时只在 0~2 跳，不适合作为航向角。
- `CMD=0x02[0]` 零偏后的 `D0` 是当前唯一可用航向候选：手动转动时左右方向有区分，回正接近 0。
- `CMD=0x01/0x03` 当前显示过的通道不适合作为航向反馈。

### `keil/user_motor/motor.c`

距离参数：

```c
#define WHEEL_CIRCUM_MM        150.8f
#define ENCODER_PPR            98
#define DIST_PER_PULSE         (WHEEL_CIRCUM_MM / ENCODER_PPR)
```

注意：`ENCODER_PPR=270` 已被 V1.2 实测基本证伪；`ENCODER_PPR=116` 是过渡修正值；当前 V1.3 由 450mm/292.5pulse 反推后改为 98。

### `keil/empty_LP_MSPM0G3507_nortos_keil.uvprojx`

- Keil 主程序路径是 `../empty.c`。
- `grayscale.c` 当前被临时移出编译列表，只保留 `grayscale.h`。
- 原因：灰度模块使用的 `GREY_x_PORT` 等宏还没在 `ti_msp_dl_config.h` 中定义，会导致 17 个编译错误。
- 后续恢复灰度前，必须先补齐灰度引脚宏、GPIO 初始化和 DriverLib API 使用。

## 已完成能力

- Keil / CMSIS-DAP / FLM 补丁 / 8 字节对齐烧录链路已跑通。
- OLED SSD1306 驱动已跑通：I2C0，PA0/PA1，7-bit 地址 `0x3C`。
- OLED 刷新已优化为每页 129 字节大包，100ms 刷新不再明显过载。
- 双轮 PWM 与方向控制已跑通。
- 左右编码器中断计数已能工作，OLED 可显示左右脉冲。
- 编码器距离标定已从 `PPR=270` 修正到当前 `PPR=98`，500mm 级测试中 OLED D 与实测距离已大致接近。
- 编码器第一层闭环已验证可用：`KP=2/MAX=80` 时，最终 `E=L-R` 多数能压到 -1~7，`C` 多数在 -2~14。
- IMU UART 原始字节能持续进入，真实帧头 `55 55` 已确认。
- IMU 解析器已切到 `55 55 CMD LEN DATA SUM`，`OK` 持续增长。
- 已确认传统 Yaw/Roll/Pitch 不可用，当前采用 D0 作为航向候选。
- D0 直线修正已接入并验证方向基本正确：最近 4 次有 3 次接近直线。

## 关键实验数据

### 编码器第一层闭环：左右轮同步已基本达成

最终验证数据：

| 次数 | D | L | R | E | C | 实测 | 偏转 | Yaw×10 |
|---:|---:|---:|---:|---:|---:|---:|---|---:|
| 1 | 573 | 377 | 370 | 6 | 12 | 510 | 几乎不偏 | 0 |
| 2 | 573 | 375 | 373 | 1 | 2 | 510 | 左偏约30° | 0 |
| 3 | 612 | 403 | 394 | 7 | 14 | 520 | 右偏约15° | -5 |
| 4 | 573 | 375 | 371 | 2 | 4 | 510 | 右偏约15° | -4 |
| 5 | 570 | 371 | 371 | -1 | -2 | 510 | 左偏约30° | 5 |

结论：编码器 P 的职责是让左右轮走得一样多，这一点已基本达成；车头角度仍偏，必须靠 IMU/灰度等航向或位置反馈。

### IMU 排障关键结论

- 静止 Rel 抖动小不代表 Yaw 是真实航向。
- 手动水平转车 30° 时，传统 Yaw 只变化约 1°，不能接 PID。
- ATK `AA FF` 解析被原始字节证伪；真实帧是 `55 55`。
- `CMD=0x06` 的 R/P/Y 当前不可用作水平航向。
- `D0 = CMD=0x02[0] - bias` 是目前唯一可用航向候选。

### D0 候选通道确认

```text
正中静止：0，-346，233，-80，-190
右转约30°：-3012，-288，341，-90，-200
回正：-75，-336，235，-86，-190
左转约30°：645，-346，153，-80，-175
```

结论：`D0` 有明显水平转向响应且回正接近 0；`G1/G2/C0/C1` 不适合作为当前航向反馈。

### D0 直线测试当前结果

反转 D0 修正方向并加大到 `HC = D0 / 40` 后：

```text
几乎没有偏移，D0=9，E=6，HC=0
往右偏45°，D0=119，E=4，HC=2
几乎没有偏移，D0=44，E=4，HC=1
往右偏10°，D0=14，E=4，HC=0
```

判断：方向已经基本正确，但偶发大偏仍存在；大偏那次停车时 `D0/E/HC` 都不大，更像起步瞬间已经甩偏，而不是后半段 PID 没修回来。

## 当前下一步

1. 不要回退到 IMU 协议猜测，不要再把传统 Yaw/Roll/Pitch 当主线。
2. 不要继续盲目加 D0 增益。
3. 先改启动段：
   ```text
   0~250ms：启用编码器 EC，禁用 D0 的 HC
   250ms 后：EC + HC 都启用
   ```
4. 把 OLED 刷新从 500ms 改到 200ms，观察起步瞬间 `D0/E/EC/HC`。
5. 再测 4 次 500mm，记录最终偏角和停车时 `D0/E/EC/HC`。
6. 如果仍偶发大偏，再查地面打滑、万向轮、重心、轮胎抓地和启动机械差异。

## 调试规则

- 一次只修当前失败的一层：现在是 D0 直线控制的起步稳定性。
- 不要现在恢复完整路线状态机、灰度巡线或蜂鸣器。
- 不要把当前偶发大偏简单归因于 D0 增益太小；最近数据更像启动瞬间问题。
- 新增或改动 C 关键函数必须写函数级注释。
- 用户口令「总结这次对话。」时，自动更新 `PROJECT_MEMORY.md`、`LONG_TERM_MEMORY.md`、`plan.md`、`ROLLING.md`、`experiment_log.md`、`debug_log.md` 和长期 telos 记忆。

## 不要踩的旧坑

```text
不要把 RAM for Algorithm 改回 0x20000000
不要重新启用 SysConfig Before Build（会覆盖 ti_msp_dl_config.c/h）
不要用补空格方式覆盖 OLED 旧字符（|= 机制，空格无效）
不要把 OLED I2C 配置到 I2C1 或 PA0/PA1 之外的引脚
编码器用 EDGE_TIME（DOWN计数器）时，中断事件必须用 CCx_DN_EVENT（不能用 CCx_UP_EVENT）
推挽输出编码器不能用导线直接短接GND测试
编码器接线前用万用表确认 VCC/GND 极性
不要用 TI XDS Debugger，用 CMSIS-DAP
不要在 Keil 根目录留同名空文件遮蔽 user_xxx 子目录头文件
不要在 Keil 弹出“保存 X.h?”时随意点确定
不要把右轮 PA24 接线遗漏（J4接口）
不要把 keil/empty.c 当主程序真相源，当前工程编译的是 ../empty.c
不要把传统 Yaw/Roll/Pitch 当当前航向真相源
不要把 ATK-IMU901 继续按 AA FF 帧解析，当前实测是 55 55
不要把偶发大偏直接归咎于 D0 增益，先查启动段
```
