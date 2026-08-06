---
comet_change: ai-quota-pilot-ux-fixes
role: technical-design
canonical_spec: openspec
---

# AIQuotaPilot UX 修复 + 改名设计文档

> Change: `ai-quota-pilot-ux-fixes`（待 `/comet-open` 确认命名）
> Date: 2026-07-26

## 1. 背景与目标

仓库目录已命名为 `AIQuotaPilot`，但 Plasmoid 包 Id 仍为 `aiUsageWatcher`，造成用户认知割裂。同时 KCM 配置对话框的供应商列表页存在三个未解决的可用性问题：

1. **插件 Id 不统一**：项目目录 `AIQuotaPilot` vs 包 Id `aiUsageWatcher`，用户问过"怎么又看到 aiUsageWatcher 这个名称"。
2. **供应商详情页 Logo 不显示**：用户称"配置页没有变化"——`ProviderEditor.qml` 顶部已有 64×64 Logo 区与 FileDialog，但当 `logoPath` 为空且 `isCustom` 时该控件整段被隐藏，需要让无 Logo 的内置供应商也显示默认首字头像。
3. **供应商列表页操作无法激活 KCM "应用"按钮**：开关切换、上移下移、删除、修改配置都应触发 KCM 框架的 Apply 按钮，但目前未生效。

本次修复同时承担一项紧急维护：补全 `cfg_customOrder` / `cfg_customOrderDefault` 属性声明（plasma-plasma 的 KConfigLoader 因找不到该属性导致 plasmashell 反复崩溃，本次已在主线修补）。

## 2. 非目标

- 不为改 Id 提供兼容路径或迁移脚本（用户接受配置丢失）。
- 不重写 KCM 框架。
- 不引入新的供应商 catalog（如 GLM 5.2 等"半成品"不在本 change 范围内）。
- 不重做 `ProvidersConfig.qml` 的 ListView。

## 3. 现状分析

### 3.1 插件 Id 现状

`package/metadata.json`：
```json
"Id": "aiUsageWatcher"
```

Plasma 系统配置中现存的小组件实例、配置项（含 KDE 钱包中保存的 MiniMax/Codex 凭据与 `Plasmoid.configuration.providers` 数据）都按 `aiUsageWatcher` 持久化。改 Id 后，旧用户需重装小组件并重新配置供应商、KDE 钱包仍可用（凭据按 URL/ProviderId 维度保存，与小部件 Id 解耦）。

### 3.2 KCM "应用"按钮机制（已查 KDE 6 官方源码）

依据：
- [`SimpleKCM.qml`](https://github.com/KDE/kcmutils/blob/master/src/qml/components/SimpleKCM.qml) —— 仅容器，无 needsSave/markDirty。
- [`SettingStateBinding.qml`](https://github.com/KDE/kcmutils/blob/master/src/qml/components/SettingStateBinding.qml) —— 依赖隐式 `kcm` 上下文对象（`KCoreConfigSkeleton`）。
- [`KCoreConfigSkeleton::ItemString::setProperty`](https://github.com/KDE/kconfig/blob/master/src/core/kcoreconfigskeleton.cpp) —— 直接覆盖 `mReference`，不调用基类的相等检查，因此**对 string 类型写入始终是 dirty**。
- [`Plasmoid::configuration()`](https://github.com/KDE/plasma-framework/blob/master/src/plasma/applet.cpp) —— 内部 `KConfigPropertyMap` 包装 `KConfigLoader`，读取 `package/contents/config/main.xml`。

`cfg_providers = value` 触发的 setter 调用链：
```
QML property write
  → KConfigPropertyMap::setProperty("providers", value)
    → KConfigLoader::ItemString::setProperty(value)
      → mReference = value.toString() (始终标脏)
        → emit configChanged()
          → host (Plasma 系统设置) 激活 Apply 按钮
```

**结论**：写 `cfg_providers = ...` 本身**应当**激活 Apply 按钮。但当前未生效，说明存在干扰：

### 3.3 当前 `syncWorkingValue()` 流程

```qml
function syncWorkingValue() {
    cfg_providers = ProviderConfig.serializeDefinitions(definitions())
    root.needsSave = true   // ← SimpleKCM 没这个属性，无效
}
```

`Component.onCompleted` 内：
```qml
const parsed = ProviderConfig.parseWorkingDefinitions(cfg_providers)
for (...) providersModel.append(definitionRow(parsed[i]))
```

**问题点**：
1. `cfg_providersDefault` 缺失，导致 KConfigLoader 解析 main.xml 时报 `Setting initial properties failed` —— 已修补。
2. `Component.onCompleted` 内不会重新写 `cfg_providers`，所以"自赋值触发标脏"的副作用不存在。
3. 写入 `cfg_providers` 的 JS 函数可能与 ListModel.set() 之间有时序竞争 —— 当 `providersModel.set()` 还未刷新到 `definitionJson` 时，`syncWorkingValue()` 就读取 definitions() 拿到旧值，导致 cfg_providers 写入"看似相同"的字符串。

## 4. 设计

### 4.1 改 Id：`aiUsageWatcher` → `AIQuotaPilot`

| 文件 | 改动 |
|------|------|
| `package/metadata.json` | `"Id": "AIQuotaPilot"` |
| `README.md` | 安装命令 `~/.local/share/plasma/plasmoids/org.kde.plasma.AIQuotaPilot/` |
| `package/contents/config/config.qml` | 无 Id 引用，无需改 |
| `docs/requirements.md` | 更新包路径示例 |

副作用：
- 旧实例配置不迁移；用户需手动重装。
- 系统托盘/面板布局中的 `aiUsageWatcher` 实例会变成空容器；引导用户用 `kpackagetool6 --remove aiUsageWatcher` + `--install package` 重建。
- **不**触碰 KDE 钱包的凭据条目（按 URL 维度存）。

### 4.2 编辑页 Logo 显示

`ProviderEditor.qml:244-316` 的 logoAvatar RowLayout 已经存在。问题：当 `logoPath` 为空且 `isCustom` 为 false 但 `ProviderRegistry.logoSvgFor(catalogId)` 返回空时，整个矩形区只显示文字字符 → 用户感知"没看到 Logo"。

修复：
- `Image.source` 计算：内置 catalog 始终走 SVG（可能为空时降级到默认 bimi-color 圆形）；自定义走 `logoPath`；两者皆空显示首字符。
- 增大点击区域热区：`MouseArea` anchors.fill parent 已生效。
- 编辑页标题栏（`SectionHeading: text: "基本信息"`）与 Logo RowLayout 之间的间距调整为 `Kirigami.Units.largeSpacing`。

### 4.3 列表页操作激活 Apply 按钮（核心）

采用**官方推荐写法**：`cfg_providers = value` 触发 setter，移除 `root.needsSave = true` 死代码。

具体改动：

1. **删除** `syncWorkingValue()` 内的 `root.needsSave = true`（`SimpleKCM` 无此属性）。
2. **消除时序竞争**：把 `syncWorkingValue()` 调用从 `providersModel.set()` 同步改为 `Qt.callLater(syncWorkingValue)`，让 ListModel 的 `definitionJson` 字段先刷新。
3. **避免不必要的标脏**：在 `Component.onCompleted` 中读 `cfg_providers` 后，如果解析出的列表与 `cfg_providers` 当前字符串相同，跳过 `syncWorkingValue`。
4. **删除现有的 `onCfg_providersChanged` 空函数**（无副作用）。

### 4.4 紧急修补（已落地）

`package/contents/ui/config/GeneralConfig.qml` 和 `ProvidersConfig.qml` 补全 `cfg_customOrder` / `cfg_customOrderDefault` 属性声明。已在本次会话前期完成，无需在本 change 重新提交。

### 4.5 文档与变更

- `docs/requirements.md` 更新 Id 引用。
- `README.md` 更新安装路径示例。
- `openspec/changes/<name>/specs/...` delta spec 记录行为变更。

## 5. 验收标准

### 5.1 改名

- [ ] `metadata.json` Id 为 `AIQuotaPilot`，`kpackagetool6 --install` 后 `plasmoidviewer` 能识别。
- [ ] 老配置实例不再被自动读取（已知；用户接受）。
- [ ] KDE 钱包中已有 MiniMax/Codex 凭据在重装后仍可被识别（按 URL 维度）。

### 5.2 Logo

- [ ] 内置供应商编辑页顶部始终显示圆形 Logo（SVG 或首字符）。
- [ ] 自定义供应商点击头像可弹出 FileDialog 选择 png/jpg/jpeg/svg/bmp/gif。
- [ ] 选择的图片路径被持久化到 `definitionJson.logoPath`。

### 5.3 Apply 按钮

- [ ] 在"供应商"配置页切换任意供应商的 Switch → KCM "应用"按钮立即可点击。
- [ ] 上移/下移（要求 `cfg_sortMode === "custom"`）→ Apply 按钮立即可点击。
- [ ] 删除（删除对话框确认后）→ Apply 按钮立即可点击。
- [ ] 编辑后切回列表页 → Apply 按钮立即可点击。
- [ ] 点击 Apply → 关闭对话框 → 重新打开 → 变更已固化（开关位置、顺序、删除结果）。
- [ ] 点击"取消" → 变更不固化。

## 6. 风险

| 风险 | 缓解 |
|------|------|
| 改 Id 致所有现存 plasmoid 实例失效 | 文档明示，README 给重装步骤 |
| `cfg_providers` 时序竞争仍残留 | Qt.callLater + 跳过相同值的写入 |
| KConfigLoader 对 string 类型标脏不可靠（极端情况） | 若 Apply 仍不亮，回退方案：用 `KCM.SettingStateBinding` 把 `cfg_providers` 的"非默认高亮"作旁路标记 |
| main.qml:68/91 的 `Cannot assign to read-only property "providers"` regression | 本 change 不直接修；在任务列表中作为 follow-up 标记 |

## 7. 任务拆分（待 writing-plans 阶段细化）

1. `metadata.json` 改 Id + README + docs 更新
2. 删除 `root.needsSave = true` + Qt.callLater 包装
3. `Component.onCompleted` 跳过无变更 sync
4. Logo Image.source 计算逻辑补全（catalogId 为空时的降级）
5. 验收：plasmawindowed 启动 → 切换开关 → Apply 按钮亮
6. 验收：kpackagetool6 --upgrade package + pkill plasmashell 重启后变更持久化

## 8. 参考

- [KCMUtils SimpleKCM.qml](https://github.com/KDE/kcmutils/blob/master/src/qml/components/SimpleKCM.qml)
- [KCMUtils SettingStateBinding.qml](https://github.com/KDE/kcmutils/blob/master/src/qml/components/SettingStateBinding.qml)
- [KConfig KCoreConfigSkeleton.cpp](https://github.com/KDE/kconfig/blob/master/src/core/kcoreconfigskeleton.cpp)
- [Plasma Applet.cpp](https://github.com/KDE/plasma-framework/blob/master/src/plasma/applet.cpp) —— `configuration()` / `configScheme()`