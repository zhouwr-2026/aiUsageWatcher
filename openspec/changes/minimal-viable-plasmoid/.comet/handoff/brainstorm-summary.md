# brainstorm-summary.md

- Change: minimal-viable-plasmoid
- Date: 2026-07-20

## 确认的技术方案

**数据层**：`package/contents/js/mockData.js` 作为 JS 模块，导出 `SEED_PROVIDERS` 数组和 `fluctuateProviders(providers)` 函数。

**三个种子供应商：**
1. 云之声Token Hub — 3 plan（5小时/7天/30天），全绿，多重置时间点
2. MiniMax（原 MiniMax · Claude，后缀剥离）— 1 plan（余额 88% 黄色），extraText
3. Codex — 1 plan（周限额 67% 绿色），503/750 次，周日重置

**内置供应商预设（后续 KCM 配置页导入用）：**
- 余额型：GLM、StepFun、SiliconFlow、OpenRouter
- 套餐型：Kimi、智谱/智谱团队版、**MiniMax**、**ZenMux**（5小时/7天/月限额，按套餐不同配额）、**火山方舟**（5小时/周/月限额）
- 订阅型：Claude、**Codex**、Gemini、**OpenCode Go**
- 可选：GitHub Copilot

**ZenMux 套餐配额参考：**
| 套餐 | 价格 | 5小时配额 | 每周最大 | 每月最大 |
|------|------|-----------|----------|----------|
| Free | $0/mo | 5 Flows | 38.64 Flows | 165.6 Flows |
| Starter | $20/mo | 50 Flows | 213.293 Flows | 914.112 Flows |
| Max | $100/mo | 300 Flows | 1,280.22 Flows | 5,486.659 Flows |
| Ultra | $200/mo | 800 Flows | 3,413.921 Flows | 14,631.091 Flows |

**Timer 驱动**：`main.qml` 内 `Timer { interval: 60000; running: true; repeat: true }`，`onTriggered` 调用 `fluctuateProviders()` 并赋值给 `providers` 属性。

**后缀剥离**：`stripProviderSuffix(name)` 在 `tightestProviderName()` 中调用，支持 ` · Claude/Codex/OpenCode/Cursor/Windsurf`。

**五个修复：**
- ProviderGroup.qml：删除重复 `border.color` 赋值
- 错误态可见性：`visible: errorText.length > 0`（不再要求 plans 为空数组）
- 无 plan 时：`tightestUsedPercent()` 返回 -1，Orb 显示灰色 `"—"`
- 移除 configGeneral.qml 无效 import
- `qmllint` 无错误

## 关键取舍与风险

| 决策 | 取舍 | 风险 |
|------|------|------|
| JS 模块 vs 内联 | 独立文件便于后续替换，但多一个 import | 无 |
| ±5% 波动 | 模拟真实变化，但颜色阈值切换可能不够频繁 | 测试时可能需要手动调大波动范围 |
| 不可变更新 | 每次 Timer 触发创建新数组触发 QML 绑定 | 轻微 GC 开销，可忽略 |

## 供应商查询逻辑

内置供应商（GLM、Kimi、MiniMax、Claude、Codex、Gemini 等）的用量查询逻辑**直接复用 cc-switch 的实现**，不重新实现。cc-switch 已覆盖的 provider 类型包括：

- **余额型**: DeepSeek、StepFun、SiliconFlow、OpenRouter
- **套餐型**: Kimi、智谱、MiniMax、ZenMux、火山方舟
- **订阅型**: Claude（本机 OAuth）、Codex、Gemini

后续接入时，将 cc-switch 的 provider 逻辑移植为 QuickJS 脚本或 C++ backend 调用。

## 测试策略

- `qmllint` 静态检查
- `plasmawindowed` 运行验证：视觉确认颜色、布局、刷新、错误态

## Spec Patch

（无，spec 已按 brainstorming 讨论内容更新为种子数据结构和 Codex 周限额）