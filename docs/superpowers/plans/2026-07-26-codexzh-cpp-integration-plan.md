---
change: codexzh-cpp-integration
design-doc: docs/superpowers/specs/2026-07-26-codexzh-cpp-integration-design.md
base-ref: c39855d025e853226db5b50c5436ca8bc2321c8d
archived-with: 2026-07-26-codexzh-cpp-integration
---

# CodexZH C++ 原生插件集成 — Implementation Plan

## 任务总览

| # | 任务 | 文件数 | 预估行数 |
|---|------|--------|---------|
| 1 | C++ 代码提取 + 构建注册 | 6 | +200/-0 |
| 2 | applet 桥接 CodexZhClient | 2 | +70/-0 |
| 3 | QML 侧接入 CodexZH snapshot | 1 | +40/-0 |
| 4 | ProviderEditor Logo 头像改造 | 1 | +30/-20 |
| 5 | providerRegistry 周限额修正 | 1 | +3/-3 |
| 6 | 编译验证 + 安装测试 | 0 | — |

## 任务 1：C++ 代码提取

从 stash@{1} 提取文件 → src/：
- src/codexzhclient.cpp (414 行, KWallet + HTTP)
- src/codexzhclient.h (87 行, Q_PROPERTY)
- src/codexzhresponseparser.cpp (193 行, JSON 解析)
- src/codexzhresponseparser.h (37 行, 结构体)

CMakeLists.txt：SOURCES 追加 4 行 + 测试目标（参考 tst_minimaxclient）

## 任务 2：applet 桥接

aiusagewatcherapplet.h：
- 添加 #include "codexzhclient.h"
- 添加 CodexZhClient m_codexzhClient 成员
- 添加 6 个 Q_PROPERTY (snapshot/loading/credentialConfigured/credentialStatus/credentialBusy/credentialError)
- 添加 3 个 Q_INVOKABLE (refreshCodexZhUsage/saveCodexZhApiKey/clearCodexZhApiKey)
- 添加 6 个 signal
- 添加 getter 方法声明

aiusagewatcherapplet.cpp：
- 初始化 m_codexzhClient(this) 在初始化列表
- 连接 6 个 signal 在构造函数
- 实现 6 个 getter + 3 个 Q_INVOKABLE

## 任务 3：QML 接入

main.qml：
- 添加 codexzhSnapshot 属性绑定
- applyCodexZhSnapshot() 函数（参考 applyMiniMaxSnapshot）
- requestCodexZhRefresh() 函数
- Connections 中监听 codexzhSnapshotChanged
- refresh() 中串联 CodexZH

## 任务 4：ProviderEditor Logo 头像改造

- GridLayout 前插入 64x64 居中头像
- 内置: Image+data:image/svg+xml 固定显示
- 自定义: MouseArea → FileDialog
- 移除 logoPath 行

## 任务 5：providerRegistry 修正

- plans: [{daily,monthly}] → [{weekly}]

## 任务 6：编译 + 安装验证

- cmake --build build
- kpackagetool6 --upgrade package
