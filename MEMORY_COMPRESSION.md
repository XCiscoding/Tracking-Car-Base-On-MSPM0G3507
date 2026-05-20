# 记忆压缩机制

最后更新：2026-05-19

## 目标

让下一个 Agent 不需要重读完整排障流水，也能快速恢复上下文。根目录固定保留三类记忆：

```text
ROLLING.md           当前交接状态，只放最新结论和下一步
PROJECT_MEMORY.md    本项目稳定事实、路径、配置、已改文件
LONG_TERM_MEMORY.md  项目内同步副本；权威长期记忆写入 C:\Users\26404\.claude\telos
```

长期记忆权威目录：

```text
C:\Users\26404\.claude\telos
```

## 触发压缩的时机

任一条件满足就压缩：

```text
ROLLING.md 超过约 250 行
出现重大成功节点，例如 Programming Done / Verify OK
一个旧假设被证伪，继续保留会误导下一任 Agent
环境重装、DFP 重装、工程配置大改之后
```

## 压缩原则

`ROLLING.md` 只保留：

```text
当前状态
成功路径
最新卡点
下一步
不要再走的旧路
```

`PROJECT_MEMORY.md` 保留：

```text
本机路径
工具链版本
Keil 工程配置
硬件接线
已改文件
最终成功日志
项目专属判断规则
```

`LONG_TERM_MEMORY.md` 保留：

```text
以后遇到同类 MSPM0G3507 / Keil / DFP / CMSIS-DAP 问题时可复用的结论
```

## 必须保留的高价值信息

不要压掉这些信息：

```text
精确路径
精确版本号
精确错误日志
最终成功日志
FLM 补丁 offset 和 bytes
已证伪的错误方向
Keil UI 中必须选择的选项
会被重装覆盖的外部文件
```

## 禁止保留的低价值信息

压缩时删掉：

```text
重复截图解释
已证伪的中间猜测
没有复现价值的临时实验
过期的“待测试”状态
无结论的长篇聊天记录
```

## 下次压缩流程

1. 先读 `ROLLING.md`、`PROJECT_MEMORY.md`、`LONG_TERM_MEMORY.md`。
2. 把新事实写入对应文件。
3. 把 `ROLLING.md` 改回短交接格式。
4. 明确标出“当前成功状态”和“下一步”。
5. 如果外部文件被改动，例如 `D:\Keil5\...\*.FLM`，必须在项目记忆里写明，因为它不在 `C:\ti\empty` 项目目录中。
