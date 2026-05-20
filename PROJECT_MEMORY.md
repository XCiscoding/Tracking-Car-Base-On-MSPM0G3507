# 项目记忆：C:\ti\empty

最后更新：2026-05-20

长期记忆权威位置：

```text
C:\Users\26404\.claude\telos
```

本项目对应长期专门文件：

```text
C:\Users\26404\.claude\telos\mspm0-keil-flash-debug.md
```

## 项目目标

在 MSPM0G3507 LaunchPad 上通过 Keil MDK 编译并烧录 OLED 示例，SSD1306 屏幕显示：

```text
Hello, ZM!
```

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

## 成功烧录条件

```text
Device: MSPM0G3507
RAM for Algorithm Start: 0x20200000
RAM for Algorithm Size:  0x1000
Flash Download: Erase Full Chip + Program + Verify + Reset and Run
Programming Algorithm: MSPM0G MAIN 128KB
Image alignment rule: Code + RO-data must be divisible by 8
Known-good size: 3152
Known-good result: Programming Done. Verify OK.
```

## 当前工程改动

`empty.c`：

- `main()` 调用 `SYSCFG_DL_init()`。
- OLED 初始化后显示 `Hello, ZM!`。
- PA7 LED 设置为高电平。
- 当前仍保留 `g_flash_program_padding[3]`。这次成功主要来自移除 printf 依赖，不是单靠 padding；后续若清理 padding，必须重新验证 `Code + RO-data` 对齐和烧录。

`ti_msp_dl_config.c/h`：

- 当前 Keil 工程实际编译的是根目录 `../ti_msp_dl_config.c`。
- 已合并 PA7 和 OLED I2C 初始化。
- OLED 使用 `I2C0`，`PA0 = IOMUX_PINCM1_PF_I2C0_SDA`，`PA1 = IOMUX_PINCM2_PF_I2C0_SCL`。
- I2C 速度降为 100 kHz，降低上拉偏弱时的风险。

`keil\user_oled\oled.c`：

- 引入 `i2c_hal.h`，通过 `DL_I2C_Master_transmit()` 发送 OLED 命令/数据。
- OLED 地址已改成明确的 7-bit 地址 `0x3C`。
- 已删除 `<stdio.h>`。
- `OLED_ShowNum()` 与 `OLED_ShowFloat()` 已改为手写格式化，避免链接 `_printf_*` 和 `locale$$data`。
- **`OLED_ShowChar`**：绘制前先清除字符格子（`|=` 机制只置位不清零，必须显式清格）。
- **`OLED_ClearLine(y)`**：清除整页 buffer（128字节），不触发刷新，供上层在覆盖写前调用。

`keil\user_oled\i2c_hal.c`：

- 阻塞发送已增加超时和错误退出，避免 OLED 未 ACK 或接线错误时卡死。

`keil\ti_msp_dl_config.c/h`：

- 与根目录配置同步为 `I2C0 + PA0/PA1 + PA7`，避免 OLED 源文件因 include 路径读到 Keil 目录旧头文件。

`keil\empty_LP_MSPM0G3507_nortos_keil.uvprojx`：

- Device 是 `MSPM0G3507`。
- `BeforeMake/RunUserProg1` 已设为 `0`，SysConfig Before Build 禁用。
- Flash Algorithm RAM 已是 `-FD20200000 -FC1000`。

`keil\empty_LP_MSPM0G3507_nortos_keil.uvoptx`：

- Flash Algorithm RAM 同步为 `-FD20200000 -FC1000`。

DFP FLM：

- 文件：

```text
D:\Keil5\ARM\PACK\TexasInstruments\MSPM0G1X0X_G3X0X_DFP\1.3.1\02_Flash_Programming\FlashARM\MSPM0G1X0X_G3X0X_MAIN_128KB.FLM
```

- 已打 clear-status 补丁。
- 备份：

```text
MSPM0G1X0X_G3X0X_MAIN_128KB.FLM.repatch_20260519_102026.bak
```

## 项目内最重要的判断规则

如果以后又出现：

```text
Full Chip Erase Done.
Programming Failed!
Error: Flash Download failed - "Cortex-M0+"
```

优先按这个顺序查：

1. Rebuild 后 `Code + RO-data` 是否能被 8 整除。
2. map 文件中是否又出现 `_printf_*` 或 `locale$$data`。
3. FLM 是否因为重装 DFP 被覆盖回原版。
4. Flash Download 是否又改成了 `Erase Sectors`。
5. RAM for Algorithm 是否仍为 `0x20200000 + 0x1000`。

## OLED 接线

```text
OLED SDA -> PA0
OLED SCL -> PA1
OLED VCC -> 3.3V
OLED GND -> GND
I2C address -> 0x3C
MCU peripheral -> I2C0
```

注意：PA0/PA1 在 MSPM0G350X 设备头文件中对应 `I2C0_SDA/SCL`，不是 `I2C1`。若代码初始化 I2C1，PA0/PA1 不会产生正确 I2C 波形。

## OLED 不亮排障规则

当 PA7 已经能置高但 OLED 不亮时，不要先回到烧录链路。按这个顺序查：

1. `main()` 是否已执行：PA7 能置高就是强证据。
2. Keil 实际编译的 `ti_msp_dl_config.c` 是哪一份：当前是根目录 `../ti_msp_dl_config.c`。
3. include 到的 `ti_msp_dl_config.h` 与实际编译的 `.c` 是否一致：当前必须同步根目录和 `keil/` 目录两份。
4. pinmux 是否来自芯片头文件事实：PA0/PA1 对应 `I2C0_SDA/SCL`。
5. OLED 地址是否用 7-bit `0x3C` 传给 DriverLib，不要再混用 8-bit 写地址 `0x78`。
6. PA0/PA1 是否有 I2C 波形：无波形查 I2C 初始化和 pinmux；有波形无 ACK 查地址、接线、上拉和供电。

## 本次重要决策日志

用户侧关键决策：

1. 决定暂停继续零散修 bug，转为重装和重建整个 Keil/CARE 环境。原因：前面问题同时涉及 Pack、Keil、DFP、SysConfig、Flash Algorithm、烧录配置，继续局部修复会让变量过多。
2. 明确给出本地安装包路径，包括 `MDK539.EXE` 和 `TexasInstruments.MSPM0G1X0X_G3X0X_DFP.1.3.1.pack`。原因：把问题从“网上找版本”收敛为“验证本机安装链路”。
3. 通过截图和完整 Build/Download 日志连续反馈实际现象。原因：Keil GUI 报错、Pack Installer 状态、Build Output 三者的信息不同，必须合并判断。
4. 接受先禁用 Keil Before Build 的 SysConfig 命令。原因：当前 `ti_msp_dl_config.c/h` 已经手写 I2C/GPIO 配置，继续自动生成会覆盖手动修复；短期目标是先烧录成功。
5. 按步骤测试 `Under Reset`、`0x1000` RAM for Algorithm、`Erase Full Chip`、不同镜像大小。原因：把 erase timeout、program failure、镜像对齐问题逐层分离。
6. 最后用 Rebuild 后的 `3152` 和 `Programming Done / Verify OK` 确认成功。原因：这两个信号共同证明不是偶然擦除成功，而是完整编程和校验成功。
7. 在 PA7 已能置高后，明确提出“烧录可能没问题，但是 OLED 的代码有问题”。原因：PA7 是程序已运行的硬证据，能把问题从烧录链路切到 OLED/I2C 运行链路。
8. 在 OLED 修好后要求沉淀步骤、决策和可抽象能力。原因：这次成功依赖一条高价值判断链，必须让下一个 Agent 复用，而不是重新从 Keil/DFP 猜起。

Agent 侧关键决策：

1. 不再继续建议重装 Keil 本体。原因：CMSIS-DAP 已识别 SW-DP，Device Pack 已识别 MSPM0G3507，问题已经从安装层转移到 Flash Algorithm 和镜像内容层。
2. 把 Debugger 方向固定为 `CMSIS-DAP Debugger`，不走 `TI XDS Debugger`。原因：Keil 32 位进程与 TI XDS 驱动链路存在 DLL/位数风险；CMSIS-DAP 已经实际连通。
3. 把 RAM for Algorithm 固定为 `0x20200000 + 0x1000`。原因：MSPM0G3507 SRAM 基地址不是常见的 `0x20000000`，错误地址会导致 erase/program 异常。
4. 保留 patched FLM，但要求用 `Erase Full Chip`。原因：补丁修复 ProgramPage clear status，但复用了原 `unprotectSector` 空间，可能影响 `Erase Sectors`。
5. 从“Flash 算法坏”进一步下钻到“ProgramPage 长度 8 字节对齐”。原因：Full Chip Erase 已完成但 Programming Failed，且不同 `Code + RO-data` 值与失败稳定相关。
6. 不再单靠 padding 解决尾部对齐，而是移除 `oled.c` 的 `snprintf/printf` 依赖。原因：map 尾部出现 `_printf_*` / `locale$$data`，库尾部改变了镜像布局；去掉依赖后尺寸变为 `3152` 并成功。
7. 成功后立即压缩滚动文件并建立 `PROJECT_MEMORY.md`、`LONG_TERM_MEMORY.md`、`MEMORY_COMPRESSION.md`。原因：旧排障流水中有大量已证伪假设，下一个 Agent 需要的是成功路径和判断规则。
8. 接受用户“顺着我的思路”的方向，把 PA7 置高视为启动链路证据。原因：这能避免把已证实可运行的问题重新归因到烧录失败。
9. 第一时间检查 Keil 工程文件和 `.d` 依赖文件，确认实际编译的是根目录 `../ti_msp_dl_config.c`。原因：嵌入式工程常有多份同名配置文件，必须先确定编译真源。
10. 对比根目录与 `keil/` 目录两套 `ti_msp_dl_config.c/h`。原因：发现了“头文件里有 I2C 宏，但真正编译的初始化函数没有 I2C 初始化”的错配。
11. 不相信旧注释里的 `PA0/PA1 = I2C1`，转而查 SDK 设备头文件 `mspm0g350x.h`。原因：pinmux 必须以芯片头文件/数据手册事实为准；结果确认 PA0/PA1 对应 `I2C0`。
12. 把修复做成最小闭环：统一配置真源、改成 I2C0、保留 PA7、OLED 地址改为 7-bit `0x3C`、I2C HAL 加超时。原因：既修正根因，又保留启动指示和后续排障可观测性。
13. 用 `armclang` 独立编译修改文件而不是强行关闭用户的 UV4 GUI。原因：当已有 Keil GUI 进程占用时，命令行 Rebuild 没刷新对象文件；源码级编译可以先验证 API/宏正确性，同时不破坏用户现场。

## 可从本项目抽出的技能候选

这些不是已经安装的 Codex Skill 文件，而是已经沉淀到本项目记忆里的可复用能力；如果以后要正式创建 Skill，可按这些名字拆分：

```text
embedded-keil-pack-recovery
用途：处理 Keil Pack Installer、DFP 安装、乱码路径、Device not found。

mspm0-keil-flash-debug
用途：处理 MSPM0G3507/MSPM0G 系列在 Keil 下 Erase/Program/Verify 失败。

flm-binary-patch-forensics
用途：对 Keil FLM 算法做备份、定位、二进制补丁、补丁后副作用记录。

embedded-image-alignment-debug
用途：用 Build Output 和 map 文件判断镜像尾部、Code+RO-data 对齐、printf/locale 链接污染。

sysconfig-vs-manual-config-control
用途：判断什么时候禁用 SysConfig 自动生成，什么时候把 .syscfg 重新作为真源。

embedded-oled-i2c-pinmux-debug
用途：处理“程序已运行但 OLED 不亮”的问题；用 GPIO 运行证据、I2C 波形、7-bit 地址、pinmux 真值、ACK/NACK 分层定位。

duplicate-config-truth-source-forensics
用途：处理同名配置文件多份存在、include 路径和实际编译源文件不一致的问题；用工程文件、.d 依赖、map 符号确认真正生效的文件。

agent-rolling-memory-compression
用途：长排障会话结束后，把流水压缩成滚动文件、项目记忆、长期记忆三层结构。
```
