# 长期记忆：MSPM0G3507 + Keil + DFP 烧录排障模式

最后更新：2026-05-19

权威保存位置：

```text
C:\Users\26404\.claude\telos\mspm0-keil-flash-debug.md
```

说明：项目内此文件是同步副本；长期记忆以 `C:\Users\26404\.claude\telos` 为准。

## 可复用结论

以后遇到 MSPM0G3507 / MSPM0G1X0X_G3X0X / Keil MDK 下载失败，先套用这套判断。若下载已成功但外设不工作，要切换到运行证据、配置真源、pinmux 和总线波形排障，不要继续把问题归因到烧录。

## Pack 安装

TI DFP `.pack` 文件名必须保持官方格式：

```text
TexasInstruments.MSPM0G1X0X_G3X0X_DFP.1.3.1.pack
```

Pack Installer 可能无法处理中文、网盘、过深路径或乱码路径。稳妥做法：

1. 把 `.pack` 复制到 ASCII 短路径，例如 `C:\ti\packs\`。
2. 用 Keil Pack Installer Import。
3. 安装后确认 Keil Device 列表能看到 `MSPM0G3507`。

## Debugger 选择

LP-MSPM0G3507 的板载 XDS110 在 Keil 下走：

```text
CMSIS-DAP Debugger
```

不要优先选 `TI XDS Debugger`。Keil 是 32 位进程，TI XDS 相关驱动版本容易出现 DLL/位数不匹配。CMSIS-DAP 能看到 `ARM CoreSight SW-DP` 和 IDCODE 时，链路已经通了。

## SRAM 地址

MSPM0G3507 的 SRAM 基地址是：

```text
0x20200000
```

Keil Flash Algorithm 不能用常见 Cortex-M 默认的 `0x20000000`。错误 RAM 地址会导致 erase/program 异常。

## DFP 1.3.1 FLM 问题

`MSPM0G1X0X_G3X0X_MAIN_128KB.FLM` 的 ProgramPage 路径存在缺陷：缺少 clear status。表现为 erase 能动，但 program 失败。

已验证的本机补丁：

```text
offset 0x150:
00 F0 5A F8 C0 46 C0 46 C0 46 C0 46

offset 0x208:
2D 22 12 01 B8 1A 05 22 42 60 01 22 02 60
39 68 49 07 FC D4 02 22 12 02 B9 1A 00 22
0A 60 4A 60 70 47 C0 46
```

补丁后优先用：

```text
Erase Full Chip + Program + Verify
```

不要优先用 `Erase Sectors`，因为补丁复用了原 `unprotectSector` 空间，可能影响 sector erase 路径。

## 8 字节对齐规则

patched FLM 仍要求写入镜像尾部满足 8 字节对齐。判断方式：

```text
Build Output: Program Size: Code=... RO-data=...
检查 Code + RO-data 是否能被 8 整除
```

典型失败值：

```text
7540
7548
```

典型成功值：

```text
3152
```

如果不能被 8 整除，优先查 map 文件尾部是否有：

```text
_printf_*
locale$$data
snprintf
printf
```

这类库会把镜像尾部拉到不稳定位置。嵌入式显示数字时，优先手写整数/小数格式化，不要引入 `snprintf`。

## SysConfig 规则

如果项目中手写了 `ti_msp_dl_config.c/h`，禁用 Keil Before Build 的 SysConfig 命令，否则它可能覆盖手写 I2C/GPIO 配置。

只有在决定让 `.syscfg` 成为真源时，才重新启用 SysConfig，并把所有手写配置迁回 `.syscfg`。

## OLED / I2C 运行期排障

当程序已能控制某个 GPIO，例如 PA7 能置高，但 OLED 不亮时：

1. 先承认烧录和启动链路大概率已经通了，把主线切到外设代码和硬件总线。
2. 查工程文件和 `.d` 依赖，确认实际编译进来的配置 `.c` 文件是哪一份。
3. 查 include 路径和 `.d` 依赖，确认外设源文件看到的配置 `.h` 是否与实际编译的 `.c` 同源。
4. 查芯片设备头文件或数据手册确认 pinmux，不要相信旧注释或复制来的配置。
5. DriverLib I2C API 通常使用 7-bit 地址；SSD1306 常用地址应传 `0x3C`，不要在 DriverLib 层混用 8-bit 写地址 `0x78`。
6. I2C 阻塞发送应有超时和错误退出，否则地址/接线/上拉错误会把程序卡死，掩盖真实问题。
7. 若还不亮，用逻辑分析仪/示波器看 SDA/SCL：无波形查初始化和 pinmux；有波形无 ACK 查地址、接线、上拉和供电。

本项目的已验证事实：

```text
MSPM0G350X PA0 = IOMUX_PINCM1_PF_I2C0_SDA
MSPM0G350X PA1 = IOMUX_PINCM2_PF_I2C0_SCL
不要把 PA0/PA1 配成 I2C1。
```

## 多份配置文件真源规则

嵌入式工程里出现多份同名 `ti_msp_dl_config.c/h` 时，必须先找“真正生效的文件”：

1. 读 Keil `.uvprojx`，看源文件路径。
2. 读 `Objects/*.d`，确认每个 `.o` 实际依赖哪个 `.c/.h`。
3. 读 `.map`，确认关键初始化函数是否进入镜像。
4. 不要只看某个头文件里有没有宏；头文件宏和实际链接进来的初始化函数可能来自不同目录。
5. 短期修复要同步所有会被 include/compile 的副本；长期维护要收敛成单一真源，或重新让 `.syscfg` 成为真源。

## 排障优先级

遇到 `Flash Timeout` 或 `Programming Failed` 时，按这个顺序排：

1. Device Pack 是否安装，Keil 是否能选到具体芯片。
2. Debugger 是否为 CMSIS-DAP，是否能识别 SW-DP。
3. RAM for Algorithm 是否为 `0x20200000 + 0x1000`。
4. Flash Download 是否为 `Erase Full Chip + Program + Verify`。
5. FLM 补丁是否仍在，重装 DFP 后必须重打。
6. `Code + RO-data` 是否 8 字节对齐。
7. 是否引入了 printf/snprintf/locale。
8. 仍失败时，再考虑断电、Under Reset、降低 SWD Clock、断开外设、UniFlash BSL 全片擦除。

下载已成功但外设不工作时，按这个顺序排：

1. 找一个 GPIO 或其它可观测信号确认 `main()` 是否执行。
2. 确认工程实际编译的配置源文件。
3. 对比配置头文件与配置源文件是否漂移。
4. 核对 pinmux 的真实外设编号。
5. 核对总线地址格式，例如 I2C 7-bit/8-bit 地址。
6. 看总线波形与 ACK/NACK。
7. 最后再回头考虑电源、模块损坏或烧录链路回归。

## MSPM0 小车 IMU 与直线控制经验（2026-05-23）

- ATK-IMU901 六轴串口版不能只凭模块名假设 `AA FF` 帧；本项目实测真实输出是 `55 55 CMD LEN DATA SUM`，`CMD` 在 `0x01/0x02/0x03/0x06` 间切换。
- `CMD=0x06` 的传统 Roll/Pitch/Yaw 即使能解析，也可能不对应水平车头航向；ATK-MS901M 说明书姿态角比例是 `raw / 32768 * 180`，但比例修正只能解决量纲问题，不能替代轴映射验证。本项目手动水平转 30° 时 R/P/Y 只小幅跳动，不可直接接 PID。
- 真正可用的航向候选来自原始通道：`D0 = CMD=0x02[0] - bias`。先用手动右转/回正/左转验证“左右反号、回正接近 0”，再接直线控制。
- D0 接入直线控制后，不要只看停车读数判断问题。若停车时 `D0/E/HC` 都不大但车曾大偏，问题可能发生在起步瞬间，需要观察起步过程。
- 启动段不要粗暴关闭所有修正。更稳的分层策略是：起步前 250ms 先开编码器同步 EC、暂关 IMU 航向 HC；等轮子进入稳定滚动后再开启 EC+HC。

## 可抽象成 Codex 技能的能力

以后如果正式创建 Skill，优先把本次经验拆成这些技能。每个技能都应保持短小，详细芯片/路径放 references，必要时把补丁校验脚本放 scripts。

### embedded-keil-pack-recovery

触发场景：

```text
Keil 找不到芯片
Pack Installer 打不开 .pack
Pack Installer 乱码
bad pack name
Requested device ... not found
```

核心流程：

1. 先确认 `.pack` 文件名是否仍是官方 `Vendor.Pack.x.y.z.pack` 格式。
2. 避免中文、网盘、超长路径；复制到 ASCII 短路径再 Import。
3. 安装后不看“是否点过安装”，而看 Keil Device 列表能否选到具体芯片。
4. 遇到 Pack Installer 管理员实例占用，先关闭所有 Pack Installer/uVision，再重试。

### mspm0-keil-flash-debug

触发场景：

```text
MSPM0G3507
CMSIS-DAP
Flash Timeout
Erase Failed
Programming Failed
Target DLL has been cancelled
Cortex-M0+
```

核心流程：

1. 固定 Debugger 为 CMSIS-DAP，确认能看到 SW-DP/IDCODE。
2. 固定 RAM for Algorithm 为 `0x20200000 + 0x1000`。
3. 优先 `Erase Full Chip + Program + Verify`。
4. 如果 erase 成功但 program 失败，马上转向 FLM 补丁和镜像 8 字节对齐，不要重复重装 Keil。

### flm-binary-patch-forensics

触发场景：

```text
Keil FLM 算法疑似有厂商缺陷
DFP 重装后问题复发
需要改 .FLM 二进制
```

核心流程：

1. 先备份原始 `.FLM`，记录 DFP 版本和文件路径。
2. 记录 patch offset、bytes、目的和副作用。
3. 重装 DFP 后默认认为 FLM 被还原，必须重新校验补丁。
4. 补丁不能只记录“已修好”，必须记录哪些 Keil 操作不能再用；本例是不要优先用 `Erase Sectors`。

### embedded-image-alignment-debug

触发场景：

```text
Erase Done / Full Chip Erase Done
Programming Failed
Build Output 中 Code/RO-data 变化
map 尾部出现 printf/locale
```

核心流程：

1. 计算 `Code + RO-data`。
2. 检查是否满足目标 flash writer 的写入粒度；本例为 8 字节。
3. 如果不对齐，先看 map 尾部，不要盲目加 padding。
4. 若出现 `_printf_*`、`locale$$data`、`snprintf`，优先移除标准库格式化，改手写轻量格式化。
5. 每次 Rebuild 后重新计算，不沿用旧尺寸。

### sysconfig-vs-manual-config-control

触发场景：

```text
TI SysConfig
Before Build user command
手写 ti_msp_dl_config.c/h
自动生成覆盖手写代码
```

核心流程：

1. 先判断真源：是 `.syscfg` 还是手写 `ti_msp_dl_config.c/h`。
2. 若短期目标是保住手写配置，禁用 Before Build 的 SysConfig。
3. 若长期目标是可维护生成配置，把手写配置迁回 `.syscfg` 后再启用。
4. 不允许同时让 SysConfig 自动生成，又在生成文件里长期手写关键配置。

### embedded-oled-i2c-pinmux-debug

触发场景：

```text
程序能跑，GPIO 能翻转或置高
OLED 不亮
I2C 设备无响应
SDA/SCL 没波形或无 ACK
```

核心流程：

1. 用 GPIO 运行证据切断“烧录失败”假设。
2. 确认 OLED 驱动传给 DriverLib 的是 7-bit 地址。
3. 查芯片头文件确认 SDA/SCL 对应的 I2C 外设编号和 pinmux function。
4. 给 I2C 阻塞发送加超时，避免卡死。
5. 先用 100 kHz 降低上拉和连线风险；稳定后再提速。
6. 用波形把问题分成“没发出”“发出但无 ACK”“ACK 了但初始化/显存不对”。

### duplicate-config-truth-source-forensics

触发场景：

```text
工程里有多份同名配置文件
include 路径和工程源文件路径不一致
头文件里有宏但运行行为不符合
SysConfig 生成文件和手写文件混用
```

核心流程：

1. `.uvprojx` 定位被编译的 `.c`。
2. `Objects/*.d` 定位每个 `.o` include 的 `.h`。
3. `.map` 定位实际链接进镜像的初始化函数。
4. 不要被注释和文件名误导，以编译依赖和 map 为准。
5. 短期同步副本，长期收敛真源。

### agent-rolling-memory-compression

触发场景：

```text
长时间排障
多轮假设被证伪
成功节点已经出现
用户要求下一个 Agent 能快速接手
```

核心流程：

1. 把当前成功状态放进 `ROLLING.md` 顶部。
2. 把项目固定事实放进 `PROJECT_MEMORY.md`。
3. 把跨项目规律放进 `LONG_TERM_MEMORY.md`。
4. 删除或压缩已证伪旧假设，保留“不要再走的旧路”。
5. 保留精确路径、版本、日志、补丁字节、最终成功信号。

## 这次要长期记住的决策原则

### MSPM0 小车直线调试原则（2026-05-22）

1. 直线控制不要一上来写 PID。先按顺序做：编码器距离标定 → 左右轮开环 trim → IMU Yaw 单独验证 → 最后恢复 P/PI/PID 闭环。
2. 当 OLED 编码器距离和地面实测距离差 2 倍以上时，优先重算 `DIST_PER_PULSE` / `ENCODER_PPR`，不要继续调 Kp。
3. 左右脉冲接近但车仍明显偏转，说明问题不只是 PWM 静态比例，可能有打滑、万向轮、重心、地面摩擦或启动瞬间差异。
4. 肉眼偏角明显但 IMU Yaw 显示接近 0 时，不要把 Yaw 当真相源；先做原地手动旋转角度验证。
5. 实验数据要记录到项目文档，字段至少包括：OLED D、左右脉冲、实测距离、偏转方向/角度、Yaw、参数。
6. 轮径 48mm 时周长约 150.8mm；本项目 V1.2 由实测反推 `ENCODER_PPR≈116`，但仍需下一轮验证。
7. 用户口令「总结这次对话。」表示自动更新项目记忆、长期记忆、plan 文档和滚动文件，交接标准是新 Agent 能直接继续做。
8. 编码器 P 闭环只能保证左右轮累计脉冲接近，不能直接保证车头航向正确；当 `E=L-R≈0` 但车仍偏 15°~30°，不要继续盲目加编码器 Kp，应切到 IMU/灰度等航向或位置反馈。
9. 手动水平转车 30° 时，如果当前 Yaw 只变化约 1°，不要把 Yaw 接进 PID；先做 Roll/Pitch/Yaw 三轴相对角诊断，确认真实航向轴和安装方向。
10. IMU 静止抖动小不等于航向轴正确；静止 Rel≈0 只能说明短时稳定，不能说明它对应车体水平转角。
11. `CMD=55` 的 payload D/E 已被实测证伪，不是可用航向角速度；不要把稳定出现的 CMD 直接当成可用运动量。
12. `RAW` 持续增加但 `OK=0` 的含义是 UART 接收链路通、当前协议解析不命中。此时不要继续凭模块名猜 `CMD` 或帧格式，必须先显示最近 8 个原始串口字节，再按真实帧头、长度和校验规则重建解析器。

用户的有效决策模式：

1. 当局部修复无法收敛时，主动切换到环境重建和变量归零。
2. 提供精确路径、截图、完整日志，而不是只描述“还是不行”。
3. 每次只改一个关键变量，然后回传结果，例如 `Full Chip Erase`、`Under Reset`、镜像尺寸。
4. 在成功后要求立刻沉淀记忆，避免下一次重复排障。
5. 当有新的硬证据时，主动要求切换排障主线；本例是 PA7 能置高后，把问题从烧录链路切到 OLED 代码。
6. 在修复有效后立即要求抽象能力和记录决策原因，避免只留下“改了几个文件”的表层记忆。

Agent 的有效决策模式：

1. GUI 工具问题要按层拆：安装层、设备识别层、调试连接层、擦除层、编程层、镜像内容层。
2. 连接已经成功时，不要继续建议重装 IDE；要把注意力转到下游失败点。
3. 看到 `Erase Done` 后的 `Programming Failed`，要区分 erase algorithm 和 program algorithm。
4. 看到镜像尺寸反复变化时，要查 map 和链接库，不要只用 padding 猜。
5. 对嵌入式小程序，`printf/snprintf` 是高风险依赖；它可能引入 locale、浮点格式化、尾部对齐和体积问题。
6. 成功后要把“为什么成功”写清楚，不只写“成功了”。
7. 用户给出有效方向时要顺着硬证据推进，而不是固守上一阶段的排障路径。
8. 对外设不工作问题，先找可观测运行证据，再查工程真源、pinmux、地址格式和波形。
9. 多份配置文件同时存在时，必须用 `.uvprojx`、`.d`、`.map` 建立事实链。
