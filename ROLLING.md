# Agent 交接滚动文件：IMU 字节已收到，前8字节全 0x00，待排查接线/协议

最后更新：2026-05-21（第五次 Session）
当前结论：IMU UART1 硬件路径通（B 计数器 9600 baud 下增加），但所有字节为 0x00，帧解析 F=0。imu.c/imu.h 框架已完成。阻塞点：IMU TX 未确认接到 MCU PB7，或协议头不匹配（0xAA 0xFF vs 0x55 0x53）。

---

## 2026-05-21 Session 总结（第五次）：IMU 驱动框架完成，诊断阶段阻塞

### 完成内容

1. **UART1 硬件配置完成**（`keil/ti_msp_dl_config.c/h`）：
   - PB6 = MCU UART1_TX → 接 IMU RX（PINCM23）
   - PB7 = MCU UART1_RX → 接 IMU TX（PINCM24）
   - 波特率从 115200 改为 **9600**（确认 B 计数器增加）
   - RX 中断已使能，NVIC priority=2，FIFO ONE_ENTRY

2. **imu.c/imu.h 完整实现**（`keil/user_imu/`）：
   - UART1_IRQHandler 帧解析状态机
   - 当前协议：ATK-IMU901 专有 `[0xAA][0xFF][CMD][LEN][DATA][CKSUM]`，欧拉角帧 CMD=0x44，LEN=6，单位 0.01°
   - 诊断 API：`IMU_GetRawByteCount()` / `IMU_GetRawCapture(buf, n)`（前8字节快照）
   - 完整 API：`IMU_Init/GetYaw/GetRoll/GetPitch/ResetYaw/IsDataReady`

3. **empty.c 改为 IMU 诊断程序**（根目录 `empty.c`）：
   - OLED 4 行布局：Yaw/Roll/Pitch 动态角度 + `B:XXXXX F:XXXX` + 前4字节 Hex
   - main() 加 `delay_ms(500)` 等模块上电 + 发 `0xFF` 唤醒字节

4. **硬件验证进展**：
   - 9600 baud：B 增加 ✅（路径通，排除硬件断线）
   - 前8字节全 0x00，F=0 ❌（协议不匹配 or 接线错误）

### 当前阻塞：0x00 字节问题诊断清单

按优先级排查：
1. **万用表测 IMU TX 引脚对 GND 电压**（空闲应 ~3V；0V = 无电 or 接 GND）
2. **确认 IMU TX 物理接到 MCU PB7**（UART1_RX），不是 PB6（TX 和 RX 要交叉）
3. **若接线正确但仍 0x00** → 协议不匹配，改为 WIT Motion 协议：
   ```
   帧头: [0x55][0x53]，共11字节
   [RollL][RollH][PitchL][PitchH][YawL][YawH][TL][TH][SUM]
   SUM = (0x53+所有DATA) & 0xFF；单位 int16 × 0.1°
   ```

### 关键接线规则（新增教训）

```
IMU TX → MCU RX（PB7）
IMU RX → MCU TX（PB6）
TX 和 RX 必须交叉，绝不能同名相连
空闲时 TX 线为高电平（~3V），0V = 断线 or 接 GND
```

### 改动文件清单

```text
c:\ti\empty\empty.c（根目录，Keil 以 ../empty.c 引用）
    - 改为 IMU 诊断程序
    - main() 加 delay_ms(500) + 发 0xFF 唤醒
    - OLED 显示 Yaw/Roll + B/F/Hex

keil/user_imu/imu.c   ← 新增（完整帧解析状态机）
keil/user_imu/imu.h   ← 新增（完整 API 声明）
keil/ti_msp_dl_config.h ← UART_IMU_BAUD 改为 9600
```

### 下一步（解除阻塞后按顺序执行）

1. 确认 Yaw 静止稳定（≤±0.5°）+ 顺时针旋转方向正负
2. 实现 `Motor_GoStraightDistance(mm, duty)` —— Yaw 闭环 P + 编码器停车
3. 实现蜂鸣器 `Buzzer_Init()` / `Buzzer_Beep(n)`（PB17，低电平触发）
4. Task1 直线走通（A→B 1000mm，到达鸣笛）
5. 灰度传感器引脚确定 → 巡线驱动 → Task2 圆弧

---

## 2026-05-21 Session 总结（第四次）：编码器调试全过程，脉冲验证成功

### 完成内容

1. **empty.c 主函数重写为编码器纯调试模式**：去掉双阶段跑车逻辑，改为：
   - `Encoder_Reset()` 清零
   - `while(1) { oled_show_encoder(); delay_ms(200); }` 实时刷新
   - OLED 显示：`L:xxxxxx R:xxxxxx Avg:xxxxx`

2. **编码器中断事件类型确认（重要）**：
   - `DL_TIMER_CAPTURE_MODE_EDGE_TIME` = **DOWN 计数器**（SDK 注释明确）
   - DOWN 计数器捕获上升沿时，产生的是 `CC0_DN_EVENT`，不是 `CC0_UP_EVENT`
   - 曾误改 DN→UP（引入新 bug），后通过读 `dl_timer.c` 源码确认后还原
   - **最终正确配置**：`EDGE_TIME` + `RISING` + `CC0_DN_EVENT` / `CC1_DN_EVENT`

3. **根因：编码器 VCC/GND 反接**：
   - 现象：浮空时脉冲累积，接信号线后脉冲立停，转轮无反应
   - 真正根因不是软件，而是接线错误——编码器的 VCC 和 GND 接反了
   - 用万用表逐根测量各线：恒定 ~3V→VCC，恒定 0V→GND，转动有切换→A 相
   - 用户将 VCC/GND 对调后，A 相信号恢复正常 0-3.3V 切换

4. **左轮编码器实测验证通过**：烧录后缓慢转左轮，OLED `L:` 行数字正常累积 ✅

5. **新增 API**：
   - `Encoder_GetLeftPulse()` — 返回左轮原始脉冲计数（uint32_t）
   - `Encoder_GetRightPulse()` — 返回右轮原始脉冲计数（uint32_t）
   - `Motor_GetTickMs()` — 返回 SysTick 毫秒计数（用于超时保护）

### 关键教训（已写入 Bug 手册）

```
Bug 16: EDGE_TIME = DOWN 计数器 → CC0_DN_EVENT
        EDGE_TIME_UP = UP 计数器 → CC0_UP_EVENT
        UP/DN 指计数方向，与边沿极性无关。遇到此类问题先读 dl_timer.c 源码。

Bug 17: 浮空有计数+接信号无计数 → 先用万用表确认信号线是否有变化，再动软件配置。
        推挽输出不能直接短接 GND（会短路供电导致系统重置），只用万用表被动测量。
```

### 改动文件清单

```text
keil/empty.c (→ 同步到 root/empty.c)
    - oled_show_motor() → oled_show_encoder()（新）
    - main() 改为编码器调试循环

keil/user_motor/motor.c
    - 新增 Encoder_GetLeftPulse() / Encoder_GetRightPulse()
    - ISR 改用 clearInterruptStatus（无条件清标志，不用 IIDX 判断）
    - 最终：CC0_DN_EVENT（还原自曾错改的 CC0_UP_EVENT）

keil/user_motor/motor.h
    - 新增三个函数声明：Motor_GetTickMs / Encoder_GetLeft/RightPulse

keil/ti_msp_dl_config.c (→ 同步到 root/)
    - SYSCFG_DL_ENCODER_LEFT_init: enableInterrupt 最终为 CC0_DN_EVENT
    - SYSCFG_DL_ENCODER_RIGHT_init: enableInterrupt 最终为 CC1_DN_EVENT
```

---

## 2026-05-20 Session 总结（第三次）：右轮修复 + 竞赛架构设计

### 完成内容

1. **右轮 PA24 接线修复**：根因是 PA24（TIMA1 CC1，PWM_RIGHT）位于 LP-MSPM0G3507 的 J4 接口，用户漏接了这根线。接线后 `Motor_SetDifferential(500, 500)` 实测双轮均正常旋转。
   - 诊断思路：对称双通道一好一坏 → 软件逻辑对称性已保证 → 原因只能是硬件/接线

2. **竞赛目标确认**：2024 全国大学生电子设计竞赛 H 题《自动行驶小车》四个任务全部。

3. **完整软件架构定稿**：
   - 模块：`user_imu` / `user_buzzer` / `user_motor`（含编码器）/ `user_grayscale` / `user_oled` / `led.h`
   - 状态机：`INIT → IDLE → RUN_STRAIGHT / RUN_ARC → WAYPOINT → DONE`
   - 路线表：`RouteSegment[]` 段序列执行器

4. **IMU 漂移解决方案设计**：ATK-IMU901 六轴（无磁力计）偏航角会漂移。解决方案：弧线段有黑线，灰度传感器巡线物理纠正航向；每次从弧线段进入直线段时 `IMU_ResetYaw()`，直线段只积累 ~3s，漂移可忽略。

5. **创建 `plan.md`**：`C:\ti\empty\plan.md` 记录赛题全貌、引脚分配、模块结构、状态机、路线表、进度、禁忌事项。

### 新增经验

```
PA24 在 J4 接口（不在常见 J1/J2 区域），双路电机接线时容易漏接右轮 PWM
ATK-IMU901 是 UART 串口模块（输出欧拉角），不是裸 IMU 芯片，不需要手写融合算法
六轴 IMU 漂移靠赛道弧线段"物理复位" + IMU_ResetYaw() 解决，不需要复杂滤波
状态机 + RouteSegment 段序列表 比 按任务硬写 if/else 更具扩展性
```

### 下一步（按优先级）

1. 查 MSPM0G3507 设备头文件，确认 PB16 对应哪个 TIMG 实例（右编码器）
2. 更新 `ti_msp_dl_config.h/c` — 新增 UART1(PB6/PB7) + 右编码器 TIMG(PB16) + 蜂鸣器(PB17)
3. 实现 `user_imu/imu.c/h` — UART1 中断 + ATK-IMU901 帧解析
4. 实现 `user_buzzer/buzzer.c/h` — PB17 低电平触发
5. 右编码器并入 `motor.c`
6. 确认灰度传感器数量（候选引脚：PB2/PB3/PA27/PB20/PB24）
7. 重写 `empty.c` 为完整状态机

---

## 当前状态

目标：OLED 显示 `Hello, ZM!`，两轮 50% 跑 2 秒，停止后 OLED 显示 `L:   0 R:   0 / STOP`。

Keil 工程：`C:\ti\empty\keil\empty_LP_MSPM0G3507_nortos_keil.uvprojx`

### 主程序逻辑（empty.c）

```text
SYSCFG_DL_init()
Motor_Init()         ← SysTick + 停止 + 编码器清零
delay_ms(100)        ← OLED 上电稳定
OLED_Init()
OLED_ShowString "Hello, ZM!"
OLED_Refresh()
PA7 高电平           ← LED 确认运行

Motor_SetDifferential(500, 500)
oled_show_motor("RUNNING")    ← 统一刷新：L/R duty + 状态行

delay_ms(2000)
Motor_Stop()
oled_show_motor("STOP")       ← 同上

while(1) {}
```

### oled_show_motor 实现（empty.c 静态辅助）

```c
static void oled_show_motor(const char *status) {
    OLED_ShowString(0, 16, "L:    R:    ", 1);
    OLED_ShowNum(12, 16, Motor_GetLeftDuty(),  4, 1);
    OLED_ShowNum(54, 16, Motor_GetRightDuty(), 4, 1);
    OLED_ClearLine(24);           // 清整行防短字符串残留
    OLED_ShowString(0, 24, status, 1);
    OLED_Refresh();
}
```

---

## 2026-05-20 Session 总结：OLED 乱码三层修复

### 用户发现的问题

1. `Motor_Stop()` 和 `delay_ms(2000)` 被注释掉，电机从未实际停止
2. 停止后 OLED 显示 `STOPING` 而非 `STOP`
3. 早期尝试用 `"STOP   "` 补空格无效

### 根因分析链

**第一层（Agent 误判）**：认为补空格可以覆盖旧像素 → 无效，因为：

- `OLED_ShowChar` 使用 `|=` 写显存
- 空格字符的字库数据全为 0，`0 |= old_data` → 旧像素不变

**第二层（修复 A，`OLED_ShowChar`）**：绘制前先清字符格子。解决了同位置字符叠加问题。

**第三层（仍有残留）**：`"STOP"`（4字符）只清了 x=0~23，`"RUNNING"` 的 `"ING"` 在 x=24~41，根本没被访问 → 仍显示 `"STOPING"`。

**最终修复 B（`OLED_ClearLine`）**：在 `oled_show_motor` 里写字符串前调 `OLED_ClearLine(24)`，清除整页 128 字节 → 彻底解决。

### 改动文件清单

```text
empty.c                    ← 取消注释 delay_ms/Motor_Stop；提取 oled_show_motor
keil/user_oled/oled.c      ← OLED_ShowChar 加格子清零；新增 OLED_ClearLine
keil/user_oled/oled.h      ← 声明 OLED_ClearLine
keil/user_grayscale/grayscale.c  ← Line_Follow 改用 Motor_SetDifferential（未加入 Keil）
keil/user_grayscale/grayscale.h  ← 更新注释
```

### 可抽象的能力

```text
OLED |= 显存机制：只要驱动用 |= 写像素，就必须配套"先清后写"策略。
两级清除策略：
  - 字符级（ShowChar 内清格）：解决同位置字符叠加
  - 行级（ClearLine 后 ShowString）：解决变长字符串尾部残留
单点刷新封装：把一组相关 OLED 操作封装成一个函数，避免分散调用时漏清/漏刷。
注释代码陷阱：delay/Stop 被注释后症状看似是"显示问题"，实际根因是逻辑未执行。
```

---

## 成功配置（不变，从上一 Session 继承）

```text
MCU:       MSPM0G3507
Board:     LP-MSPM0G3507，onboard XDS110
IDE:       Keil MDK 5.39 at D:\Keil5
Compiler:  ARMCLANG V6.21
SDK:       C:\ti\mspm0_sdk_2_01_00_03
DFP:       TexasInstruments.MSPM0G1X0X_G3X0X_DFP 1.3.1（已打 FLM 补丁）
OLED:      SSD1306, I2C0, PA0=SDA, PA1=SCL, 地址 0x3C
Debugger:  CMSIS-DAP，RAM 0x20200000/0x1000
```

**烧录必查项**：`Code + RO-data` 必须能被 8 整除。已知好值：3152。

---

## 下一步

1. Keil Rebuild → 确认 0 Error，`Code + RO-data` 被 8 整除
2. Download → 观察 OLED 2 秒后从 `RUNNING` 切换到 `STOP`，数字从 500 → 0
3. 将 `grayscale.c/h` 加入 Keil 工程（需在 .uvprojx 添加源文件 + include path `.\user_grayscale`）
4. 在 `empty.c` 调用 `Line_Follow()` 做循迹测试

## 不要再走的旧路

```text
不要用补空格来覆盖旧字符（|= 机制下空格无效）
不要只做 OLED_ShowChar 级别的修复而忽略变长字符串尾部残留
不要把 delay_ms/Motor_Stop 注释着就测试显示逻辑
不要把 PA0/PA1 配成 I2C1；MSPM0G350X 上是 I2C0
不要重新启用 SysConfig Before Build
不要把 RAM for Algorithm 改回 0x20000000
```

最后更新：2026-05-20  
当前结论：发现并修复小车不动的根因——`Motor_Stop()` 和 `Motor_Forward()` 内部调用了 TIMA1（`PWM_RIGHT_INST`）和 TIMG0（`QEI_LEFT_INST`），而这两个外设从未上电，导致 `Motor_Init()` 第一行就触发 HardFault，程序完全卡死。已修复初始化，并将 `empty.c` 改为单轮测试（`Motor_SetDifferential(500, 0)`）。

已验证：Keil Rebuild → 0 Error，0 Warning，`Code + RO-data` 可被 8 整除；此前 Download 也验证过 Programming Done / Verify OK。

下一个 Agent 读取顺序：

1. 本文件 `ROLLING.md`：只放当前状态、成功步骤、下一步。
2. `PROJECT_MEMORY.md`：项目固定事实和本仓库改动。
3. `LONG_TERM_MEMORY.md`：以后遇到 MSPM0G3507 + Keil 同类问题时直接套用。
4. `MEMORY_COMPRESSION.md`：滚动文件压缩规则。

## 当前状态

目标已达成到"OLED 正常显示 + 电机驱动模块封装完成 + Keil 编译 0 错误"阶段。项目入口在：

```text
C:\ti\empty
```

Keil 工程：

```text
C:\ti\empty\keil\empty_LP_MSPM0G3507_nortos_keil.uvprojx
```

当前测试程序：

```text
C:\ti\empty\empty.c
```

OLED 目标输出：

```text
Hello, ZM!
```

本轮 OLED 侧成功修复：

```text
根目录 ti_msp_dl_config.c/h 现在初始化 I2C0 + PA0/PA1 + PA7。
keil/ti_msp_dl_config.c/h 也同步为 I2C0 + PA0/PA1 + PA7，避免 include 路径读到旧宏。
keil/user_oled/oled.c 使用明确 7-bit 地址 0x3C。
keil/user_oled/i2c_hal.c 增加超时，避免 OLED 未 ACK 时程序长期卡死。
```

本轮成功判断链：

```text
PA7 已能置高 -> main() 已执行，烧录和启动链路不是首要问题。
Keil 工程实际编译根目录 ../ti_msp_dl_config.c -> 根目录配置才是真源。
根目录配置原来只初始化 PA7 -> OLED I2C 外设实际未初始化。
keil/ti_msp_dl_config.h 原来把 PA0/PA1 写成 I2C1 -> 与 MSPM0G350X 设备头文件不符。
MSPM0G350X: PA0=IOMUX_PINCM1_PF_I2C0_SDA, PA1=IOMUX_PINCM2_PF_I2C0_SCL。
修复为 I2C0 + PA0/PA1 + 7-bit 地址 0x3C 后，用户反馈修好。
```

注意：本轮命令行 UV4 因已有 `UV4` GUI 进程存在，没有刷新 Keil 对象文件；源码级 `armclang` 独立编译已通过。硬件有效性来自用户后续反馈。


## 2026-05-20 Session 总结：电机模块封装 + Keil 编译环境调试

### 完成内容

1. **empty.c 模块化重写**：删除所有裸寄存器操作，改用封装函数：
   - `Motor_Init()` — 替代手动 SysTick 初始化 + GPIO 初始化
   - `Motor_Forward(500)` / `Motor_Stop()` — 替代裸 DL_GPIO / DL_TimerA 调用
   - `delay_ms(100/2000)` — 替代 `for(volatile i...)` 魔法循环
2. **motor.c 新增 `delay_ms()`**：基于 `g_sysTickCount`（SysTick 1ms），`Motor_Init()` 后可用
3. **motor.h 新增声明**：`void delay_ms(uint32_t ms)`

### Keil 编译环境三连 Bug 排查（重要经验）

**Bug A：`.uvprojx` IncludePath 缺少 `.\user_motor`**
- 根因：IncludePath 里只有 `.\user_oled`，没有 `.\user_motor`
- 修复：在 `.uvprojx` IncludePath 末尾追加 `;.\user_motor`

**Bug B：`keil\Motor.h` 空文件遮蔽了真正的头文件（核心 Bug）**
- 根因：`keil\` 根目录下有空文件 `Motor.h`，`-I.` 路径优先命中它；
  Windows 文件系统不区分大小写，`#include "motor.h"` 命中空文件而非 `user_motor\motor.h`
- 排查手段：读 `Objects\empty.d` 依赖文件，
  发现 `motor.h` 没有目录前缀（oled.h 有 `user_oled\` 前缀），由此定位
- 修复：`Remove-Item c:\ti\empty\keil\Motor.h`

**Bug C：Keil 弹窗"保存 Motor.h?"后空内容覆盖了 `user_motor\motor.h`**
- 根因：删除 `Motor.h` 时 Keil 仍持有其编辑状态；Rebuild 时弹出保存对话框，
  用户点确定，Keil 把空内容写入了项目配置的 `.\user_motor\motor.h`
- 修复：用 create_file 恢复 motor.h 完整内容

### Keil 弹窗问题（需用户 UI 操作）
- "Save Motor.c?" → 关闭 Keil 编辑器里的 motor.c 标签页
- "Reload .map?" → Edit → Configuration → Editor → 勾选自动重载，关闭 .map 标签页

### 当前编译状态
- 全部 0 Error，0 Warning ✅
- **尚未 Download 到硬件验证**

### 下一步
1. Keil Download → 观察左轮是否正转 2 秒后停止
2. 创建 `keil/user_motion/motion.c`，把 `grayscale.c` 里的 `Line_Follow()` 移入 motion 层，改用 `Motor_SetDifferential()`

## 本次对话总结

用户观察到 PA7 已经被程序置高，因此判断烧录可能没问题，OLED 代码更可疑。Agent 沿着这个思路把问题从 Keil/DFP/FLM 烧录链路切换到运行期外设链路，检查 Keil 工程、`.d` 依赖和 SDK 设备头文件后，发现真正生效的根目录 `ti_msp_dl_config.c` 没有初始化 I2C，而 `keil/ti_msp_dl_config.h` 里的旧 I2C1 宏又与 PA0/PA1 的真实 pinmux 不符。修复为 `I2C0 + PA0/PA1 + 7-bit 地址 0x3C` 后，用户反馈问题一次修好。

本次可抽象能力：

```text
运行证据优先：GPIO 已按预期变化时，优先把问题切到外设代码/总线，而不是继续怀疑烧录。
配置真源取证：用 .uvprojx、Objects/*.d、map 文件确认实际编译和链接的配置文件。
pinmux 事实核对：以 SDK 设备头文件/数据手册为准，不信旧注释和复制配置。
I2C 地址分层：DriverLib 侧使用 7-bit 地址，SSD1306 常见地址为 0x3C。
可观测性修复：I2C 阻塞发送加超时，保留 PA7 作为运行指示。
记忆压缩：成功后立即记录根因、决策和不要再走的旧路。
```

## 成功配置

硬件与工具链：

```text
MCU:      MSPM0G3507
Board:    LP-MSPM0G3507 LaunchPad, onboard XDS110
IDE:      Keil MDK 5.39 at D:\Keil5
Compiler: ARMCLANG V6.21
SDK:      C:\ti\mspm0_sdk_2_01_00_03
DFP:      TexasInstruments.MSPM0G1X0X_G3X0X_DFP 1.3.1
OLED:     SSD1306, I2C address 0x3C, PA0=SDA, PA1=SCL
```

Keil 必须这样选：

```text
Device: MSPM0G3507
Debug adapter: CMSIS-DAP Debugger
Port: SW
SWD Clock: 1 MHz
Connect: Under Reset 或 Normal 均可尝试；卡死时优先 Under Reset
Reset: SYSRESETREQ
```

Flash Download 页面：

```text
Erase Full Chip
Program
Verify
Reset and Run
RAM for Algorithm Start: 0x20200000
RAM for Algorithm Size:  0x1000
Programming Algorithm: MSPM0G MAIN 128KB
```

不要用 `Erase Sectors`。当前 patched FLM 复用了原 `unprotectSector` 空间，而 `EraseSector` 也可能调用该路径，容易导致 Erase Timeout。成功路径是 `Erase Full Chip + Program + Verify`。

## 最终成功步骤

1. 安装 Keil MDK 5.39。
2. 安装 TI MSPM0G3507 DFP 包，最终包目录为：

```text
D:\Keil5\ARM\PACK\TexasInstruments\MSPM0G1X0X_G3X0X_DFP\1.3.1
```

3. Keil Device 选择 `Texas Instruments -> MSPM0G1X0X_G3X0X Series -> MSPM0G350X -> MSPM0G3507`。
4. Debug 选择 `CMSIS-DAP Debugger`，不要选 `TI XDS Debugger`。
5. 禁用 Keil Before Build 的 SysConfig 命令，避免它覆盖手写配置：

```xml
<BeforeMake>
  <RunUserProg1>0</RunUserProg1>
</BeforeMake>
```

6. Flash Algorithm RAM 改为：

```text
Start = 0x20200000
Size  = 0x1000
```

7. 对 DFP 1.3.1 的 FLM 重新打补丁，修复 ProgramPage 缺少 clear status 的问题。补丁文件：

```text
D:\Keil5\ARM\PACK\TexasInstruments\MSPM0G1X0X_G3X0X_DFP\1.3.1\02_Flash_Programming\FlashARM\MSPM0G1X0X_G3X0X_MAIN_128KB.FLM
```

备份文件：

```text
MSPM0G1X0X_G3X0X_MAIN_128KB.FLM.repatch_20260519_102026.bak
```

补丁字节：

```text
offset 0x150:
00 F0 5A F8 C0 46 C0 46 C0 46 C0 46

offset 0x208:
2D 22 12 01 B8 1A 05 22 42 60 01 22 02 60
39 68 49 07 FC D4 02 22 12 02 B9 1A 00 22
0A 60 4A 60 70 47 C0 46
```

8. 去掉 `keil\user_oled\oled.c` 中的 `stdio.h` / `snprintf` 依赖，`OLED_ShowNum()` 和 `OLED_ShowFloat()` 使用手写格式化。
9. Rebuild，确认 Build Output 的：

```text
Code + RO-data
```

必须能被 8 整除。这次成功值是：

```text
3152
```

10. Keil Download，成功输出：

```text
Programming Done.
Verify OK.
```

## 这次真正的根因

烧录阶段不是单一问题，而是三层问题叠加：

1. DFP Pack 未正确安装时，Keil 找不到 `MSPM0G3507`，会报 `Requested device MSPM0G3507 not found`。
2. 原 DFP 1.3.1 的 `MSPM0G1X0X_G3X0X_MAIN_128KB.FLM` 写 Flash 路径缺少 clear status，需要二进制补丁。
3. patched FLM 的 ProgramPage 仍要求写入长度 8 字节对齐。`oled.c` 里的 `snprintf` / printf / locale 链接尾部导致 `Code + RO-data` 出现 `7540`、`7548` 这类不能被 8 整除的值，于是 Full Chip Erase 成功但 Programming Failed。

最终修复是移除 `snprintf/printf` 依赖，让镜像尾部重新对齐；Rebuild 得到 `3152`，下载成功。

OLED 不亮阶段是另外一层问题：

1. PA7 能置高说明 `main()` 运行，烧录链路不再是首要嫌疑。
2. Keil 工程编译的是根目录 `../ti_msp_dl_config.c`，而这份文件原来只初始化 PA7，没有初始化 I2C。
3. OLED 源文件因 include 路径可能读到 `keil/ti_msp_dl_config.h`，造成“头文件看起来有 I2C 宏，但实际初始化函数没有 I2C 初始化”的错觉。
4. PA0/PA1 在 MSPM0G350X 上是 `I2C0_SDA/SCL`，不是 `I2C1_SDA/SCL`。

最终修复是统一两套配置为 `I2C0 + PA0/PA1 + PA7`，并让 OLED 使用 7-bit 地址 `0x3C`。

## 已改动的关键文件

```text
C:\ti\empty\empty.c
C:\ti\empty\ti_msp_dl_config.c
C:\ti\empty\ti_msp_dl_config.h
C:\ti\empty\keil\ti_msp_dl_config.c
C:\ti\empty\keil\ti_msp_dl_config.h
C:\ti\empty\keil\user_oled\oled.c
C:\ti\empty\keil\user_oled\i2c_hal.c
C:\ti\empty\keil\empty_LP_MSPM0G3507_nortos_keil.uvprojx
C:\ti\empty\keil\empty_LP_MSPM0G3507_nortos_keil.uvoptx
D:\Keil5\ARM\PACK\TexasInstruments\MSPM0G1X0X_G3X0X_DFP\1.3.1\02_Flash_Programming\FlashARM\MSPM0G1X0X_G3X0X_MAIN_128KB.FLM
```

注意：`D:\Keil5` 不在项目目录里，但它是这次成功链路的一部分。重装 DFP 后 FLM 会被还原，需要重新打补丁。

## 下一步

1. 继续开发前，保持 Keil 烧录配置、FLM 补丁、Full Chip Erase 路径不变。
2. 后续如果再改 OLED 或加新外设，先确认 Keil 实际编译的配置真源是哪一份文件。
3. 每次 Rebuild 后仍要看 `Code + RO-data` 是否能被 8 整除。
4. 如果 OLED 后续又不亮，优先看 PA0/PA1 I2C 波形；不要先回到 Keil/DFP/FLM 排障。
5. 长期应考虑只保留一份 `ti_msp_dl_config.c/h` 真源，或把 `.syscfg` 重新变成真源，避免两套配置再次漂移。

## 不要再走的旧路

不要把问题重新归因到这些方向，除非有新的证据：

```text
不用再重装 Keil。
不用再怀疑 CMSIS-DAP 是否识别，IDCODE 已经识别到 ARM CoreSight SW-DP。
不用再把 RAM for Algorithm 改回 0x20000000，MSPM0G3507 SRAM 是 0x20200000。
不要重新启用 SysConfig Before Build。
不要把 PA7 能置高后的 OLED 不亮重新归因到烧录失败。
不要再把 PA0/PA1 配成 I2C1；MSPM0G350X 上这组引脚对应 I2C0。
不要只看 OLED 源文件 include 到的头文件，要确认 Keil 实际编译进来的 ti_msp_dl_config.c。
不要把 .pack 文件随意改名；必须保留 Vendor.Pack.x.y.z.pack 格式。
不要用中文/BaiduNetdisk 深路径直接 Import pack，必要时复制到 ASCII 短路径。
不要优先用 Erase Sectors，当前成功路径是 Full Chip Erase。
```
