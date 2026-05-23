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

当前阶段是 **灰度静态诊断 + IMU 轴映射复核准备**：编码器第一层闭环已经基本可用，IMU 协议已解析通，当前使用 `D0 = CMD=0x02[0] - bias` 作为航向候选；但当前固件已切到亚博八路巡线模块灰度静态诊断页，烧录后不会跑电机。下一轮不要直接恢复直线 PID，应先验收灰度 G/N/E 语义；若继续 IMU，应先做静态轴映射诊断，复核官方姿态角、D0/D1/D2 到底哪个通道对应小车水平航向。

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

当前是 **亚博八路巡线模块灰度静态诊断模式**，烧录后不会跑电机，只显示 6 路灰度状态。

OLED 显示：

```text
G: 6 路灰度状态，顺序 X1/X2/X4/X5/X7/X8
E: 灰度偏差，负值代表左侧检测更强，正值代表右侧检测更强
N: 当前软件统计到的检测路数
```

当前主函数入口调用 `Run_Grayscale_Diag()`，不再调用 D0 直线测试。恢复第一题前必须先把灰度 G/N/E 的真实语义验收清楚。

### 灰度模块：`keil/user_grayscale/grayscale.c/h`

已恢复 `grayscale.c` 到 Keil 工程编译列表，当前使用 **亚博八路巡线模块**，只接 6 路：

| 模块路数 | MCU 引脚 | 说明 |
|---|---|---|
| X1 | PB2 | 左一 |
| X2 | PB3 | 左二 |
| X3 | 不接 | 空 |
| X4 | PA27 | 中左 |
| X5 | PB20 | 中右 |
| X6 | 不接 | 空 |
| X7 | PB14 | 右二 |
| X8 | PB15 | 右一 |

已在 root `ti_msp_dl_config.h` 和 `keil/ti_msp_dl_config.h` 同步补齐 `GRAY_X*` 宏，并在 `ti_msp_dl_config.c` 中初始化为上拉输入。注意 Keil 编译 root `../ti_msp_dl_config.c`，所以 root 头文件才是编译真源。

当前用户接法：亚博模块要求 5V，用户使用外部 5V 供电。必须共地；若模块输出高电平为 5V，MSPM0 GPIO 有被 5V 打坏风险。用户当前选择先继续调代码。

当前代码状态：`Grayscale_Read()` 读取到低电平时置对应 bit 为 1；`N` 统计 bit=1 的数量；`Grayscale_GetError()` 也按 bit=1 的路参与权重计算。下一轮必须用 OLED 实测确认这是否符合模块真实显示：用户现场描述为“检测到黑线灯亮，G 的对应值变 0，N+1”，这个描述与当前代码仍可能存在语义冲突，不能直接接入第一题。

### `keil/user_imu/imu.c/h`

当前 IMU 真实协议已确认：

```text
[0x55][0x55][CMD][LEN][DATA...][SUM]
```

已知现象：

- `CMD` 在 `0x01/0x02/0x03/0x06` 间切换。
- `OK` 持续增长，checksum 解析已通。
- `CMD=0x06` 解析出的传统 `Roll/Pitch/Yaw` 水平转动车身时只在 0~2 跳，不适合作为航向角；原因不是模块没有姿态解析，而是模块坐标系/输出配置/磁力计融合环境不一定等于小车绕地面竖直轴的车头航向。
- ATK-MS901M 说明书官方姿态角换算为 `raw / 32768 * 180`，当前代码里 `CMD=0x06` 暂按 `raw * 0.01f` 解算；比例不一致需要后续修正验证，但主问题是实测趋势不对应，而不只是比例系数。
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
- `grayscale.c` 已加回编译列表，当前灰度诊断固件已能编译。
- 下一轮不要再说灰度模块未加入工程；真正待确认的是亚博 5V 模块的电平安全、G/N/E 语义和阈值/高度稳定性。

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

最近一轮软启动 + EC 加大后的数据：

```text
左偏30°，D0=0，E=-3，EC=-12，HC=0，LP=212，RP=188，M=1，D=440
左偏60°，D0=-7082，E=-2，EC=-6，HC=0，LP=206，RP=194，M=1，D=440
左偏15°，D0=-17021，E=-4，EC=-12，HC=0，LP=212，RP=188，M=1，D=438
左偏30°，D0=0，E=-1，EC=-3，HC=0，LP=203，RP=197，M=1，D=438
左偏10°，D0=0，E=0，EC=0，HC=0，LP=200，RP=200，M=1，D=441
左偏30°，D0=71，E=-15，EC=0，HC=0，LP=215，RP=105，M=1，D=440
```

判断：当前基本稳定左偏；`HC` 几乎一直为 0，说明 D0 修正没有实际参与。`E` 多数已经很小，编码器只是在同步左右脉冲，不能保证车头方向。最后一组 `EC=0` 但 `LP/RP=215/105` 与公式不一致，下一轮要复查 OLED 记录或刷新时序。

当前用户已把代码进一步改为 `D0_DIV=20`、`D0_MAX_CORR=80`，且实际代码中 HC 公式为 `-(D0-target_d0)/20`，试图让弱陀螺仪 P 真正介入。

## 当前下一步

1. 当前先完成灰度静态诊断，不要直接接入第一题：
   ```text
   白底/无黑线：期望 G 稳定为全未触发状态，N=0
   单路黑线：只对应 Xn 变化，N=1
   多路/横线：N 随触发路数增加
   ```
2. 重点确认亚博八路巡线模块的真实语义：用户现场描述为“黑线灯亮、G 对应值变 0、N+1”，但当前代码将低电平读成 bit=1 后再统计 N，下一轮必须先看 OLED 实测结果再定最终反相逻辑。
3. 模块工作距离约 3~4mm，悬空或距离过高时亮灯不作为有效测试；必须贴近赛道白底和黑线测试。
4. 模块用外部 5V 供电时必须和 MSPM0 共地；如果输出高电平是 5V，建议加分压/电平转换。用户当前要求直接调代码，但后续若 GPIO 读数异常或 IO 损坏，必须回到电平安全检查。
5. 若继续验证 IMU，不要上车跑 PID，先做静态轴映射诊断：同屏显示 `CMD=0x06` 的 R/P/Y、`CMD=0x02` 的 D0/D1/D2、最好再带 `CMD=0x01` 三轴。
6. IMU 静态动作顺序：正中静止、水平右转约 30°、回正、水平左转约 30°、回正、抬车头、压车头、左右侧倾。
7. IMU 判定标准：只有满足“右转明显变一边、回正接近 0、左转明显变另一边，且抬头/侧倾不大幅混入”的通道，才有资格接入直线控制；官方 `CMD=0x06` 姿态角复核时按说明书比例 `raw / 32768 * 180`，不要只沿用当前 `raw * 0.01f`。
8. 灰度通过后，再把 Task 1 从当前诊断页切回直线控制：编码器 + D0 跑 A→B，接近终点后用灰度黑线辅助停车，最后再加蜂鸣器/LED 提示。
9. 暂时不要恢复完整路线状态机和弧线巡线；第一题只需要直线到 B 点停车。

## 调试规则

- 一次只修当前失败的一层：现在主线是灰度静态诊断；IMU 只做静态轴映射复核，不要边测轴边上车跑 PID。
- 当前不要恢复完整路线状态机；先完成灰度静态诊断，再接入第一题直线末端停车。
- 不要把亚博模块悬空/离地过高时的亮灯当作赛道有效状态；该模块有效检测距离约 3~4mm。
- 不要忽略 5V 模块输出进 MSPM0 GPIO 的风险；用户当前选择继续调代码，但下轮如读数异常应先查电平和共地。
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
不要把传统 Yaw/Roll/Pitch 当当前航向真相源；如果复核官方姿态角，必须先按说明书比例和轴映射动作验证
不要把 ATK-IMU901 继续按 AA FF 帧解析，当前实测是 55 55
不要把偶发大偏直接归咎于 D0 增益，先查启动段
```
