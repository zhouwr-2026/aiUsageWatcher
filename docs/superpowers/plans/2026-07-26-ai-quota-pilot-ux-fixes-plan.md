# AIQuotaPilot UX 修复 + 改名实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 Plasmoid 包 Id 从 `aiUsageWatcher` 改为 `AIQuotaPilot`，修复供应商列表页操作无法激活 KCM "应用"按钮的 bug，并让编辑页 Logo 在所有场景下可见。

**Architecture:**
- Id 改名通过 metadata.json + README + docs 文本替换。
- Apply 按钮修复通过移除 `root.needsSave = true` 死代码、`Qt.callLater` 包装消除时序竞争、`Component.onCompleted` 跳过无变更 sync。
- Logo 修复补全 Image.source 计算的 catalogId 降级路径。

**Tech Stack:** QML / JavaScript / KDE Plasma 6 KConfigLoader / KCM.SimpleKCM

**base-ref:** `e6a14acabb2f97152e50ff84694ae8eb208c9ffb`

## Global Constraints

- 路径：仓库根 `/home/zhouwr/Project/CodeWorkspace/AIQuotaPilot`
- 包结构：`package/metadata.json` + `package/contents/{config,js,ui}/`
- KConfigLoader schema：`package/contents/config/main.xml` —— 任何新增 `cfg_*` 属性必须同时声明 `cfg_*Default`
- 修改任何 QML 后必须 `cp -rf package/. ~/.local/share/plasma/plasmoids/aiUsageWatcher/` 同步安装目录（改 Id 前的过渡期）；改 Id 后安装路径改为 `AIQuotaPilot`
- 重启 plasmashell：`systemctl --user restart plasma-plasmashell.service`
- 清 QML 缓存：`rm -rf ~/.cache/plasmawindowed/qmlcache/ ~/.cache/plasmashell/qmlcache/`
- 不得引入 C++ 代码（本 change 范围之外）
- 不得修改 KDE 钱包凭据存储层
- 不得重写 `ProvidersConfig.qml` 整体结构

---

## 文件结构

| 文件 | 状态 | 职责 |
|------|------|------|
| `package/metadata.json` | 修改 | Id 从 `aiUsageWatcher` 改 `AIQuotaPilot` |
| `README.md` | 修改 | 安装路径示例更新 |
| `docs/requirements.md` | 修改 | 包 Id 引用更新 |
| `package/contents/ui/config/ProvidersConfig.qml` | 修改 | 删 `needsSave` + 加 `Qt.callLater` + 删空 `onCfg_providersChanged` |
| `package/contents/ui/config/ProviderEditor.qml` | 修改 | Logo `Image.source` 计算补全 catalogId 降级 |

未触及：`main.qml:68/91` 的 `Cannot assign to read-only property "providers"` regression（属于 follow-up，不在本 change）。

---

## Task 1: 改 Id + 文档同步

**Files:**
- Modify: `package/metadata.json:12`
- Modify: `README.md`（含安装命令段落）
- Modify: `docs/requirements.md`（含包 Id 引用段）

**Interfaces:**
- 消费：当前 `metadata.json` 内容
- 产出：新 `metadata.json` Id 为 `AIQuotaPilot`；README/docs 同步

- [ ] **Step 1: 修改 metadata.json**

`package/metadata.json` 第 12 行 `"Id": "aiUsageWatcher"` → `"Id": "AIQuotaPilot"`

- [ ] **Step 2: 全文搜索残留引用**

```bash
rg -n "aiUsageWatcher" --type-add 'qml:*.qml' -t qml --type-add 'xml:*.xml' -t xml --type-add 'json:*.json' -t json -t md package/ docs/ README.md
```

期望输出：除 `package/metadata.json` 已经改了之外，可能还有 README/docs 的引用，逐个修正。

- [ ] **Step 3: 修改 README.md**

把安装命令段落中 `org.kde.plasma.aiUsageWatcher` 改为 `org.kde.plasma.AIQuotaPilot`：

```bash
# 安装到本地 Plasma 小部件目录
cp -r package ~/.local/share/plasma/plasmoids/org.kde.plasma.AIQuotaPilot/
```

- [ ] **Step 4: 修改 docs/requirements.md**

把所有 `aiUsageWatcher` 字面量替换为 `AIQuotaPilot`。位置（先 rg 后编辑）通常在"视觉规格"或"安装路径"段落。

- [ ] **Step 5: 同步到已安装目录并重启 plasmashell**

```bash
yes | cp -rf package/. ~/.local/share/plasma/plasmoids/aiUsageWatcher/
yes | cp -rf package/. ~/.local/share/plasma/plasmoids/AIQuotaPilot/ 2>/dev/null || true
mkdir -p ~/.local/share/plasma/plasmoids/AIQuotaPilot
yes | cp -rf package/. ~/.local/share/plasma/plasmoids/AIQuotaPilot/
systemctl --user restart plasma-plasmashell.service
sleep 4
```

- [ ] **Step 6: 验证**

```bash
grep -A1 '"Id"' package/metadata.json | head -3
# 期望："Id": "AIQuotaPilot",

systemctl --user is-active plasma-plasmashell.service
# 期望：active

pgrep -af "plasmashell --no-respawn" | head -1
# 期望：plasmashell 进程存在
```

- [ ] **Step 7: Commit**

```bash
git add package/metadata.json README.md docs/requirements.md
git commit -m "feat: rename plasmoid Id from aiUsageWatcher to AIQuotaPilot"
```

---

## Task 2: 删除 `syncWorkingValue()` 中的 `root.needsSave` 死代码

**Files:**
- Modify: `package/contents/ui/config/ProvidersConfig.qml:110-114`

**Interfaces:**
- 消费：当前 `syncWorkingValue()` 函数
- 产出：`syncWorkingValue()` 仅写 `cfg_providers`，不调用 `root.needsSave = true`

- [ ] **Step 1: 编辑 ProvidersConfig.qml**

将第 110-114 行的 `syncWorkingValue()` 函数：

```qml
function syncWorkingValue() {
    cfg_providers = ProviderConfig.serializeDefinitions(definitions())
    // 手动通知 KCM 框架有未保存变更
    root.needsSave = true
}
```

改为：

```qml
function syncWorkingValue() {
    cfg_providers = ProviderConfig.serializeDefinitions(definitions())
}
```

- [ ] **Step 2: 同步到已安装目录**

```bash
yes | cp -rf package/. ~/.local/share/plasma/plasmoids/AIQuotaPilot/
```

- [ ] **Step 3: Commit**

```bash
git add package/contents/ui/config/ProvidersConfig.qml
git commit -m "fix(providers-config): remove dead root.needsSave assignment (SimpleKCM has no such property)"
```

---

## Task 3: `Qt.callLater` 包装消除 ListModel.set 与 syncWorkingValue 的时序竞争

**Files:**
- Modify: `package/contents/ui/config/ProvidersConfig.qml`（多处：onToggled、moveProvider、deleteProvider 等）

**Interfaces:**
- 消费：现有 `providersModel.set/append/move/remove` 调用 `syncWorkingValue()` 的位置
- 产出：所有 syncWorkingValue() 调用延迟到下一帧

- [ ] **Step 1: 编辑 ProvidersConfig.qml 中所有 syncWorkingValue() 调用**

共 5 处需要改为 `Qt.callLater(root.syncWorkingValue)`：
1. `function addProvider(candidate)` 中：`root.syncWorkingValue()` → `Qt.callLater(root.syncWorkingValue)`
2. `function updateProvider(candidate, originalId)` 中：`root.syncWorkingValue()` → `Qt.callLater(root.syncWorkingValue)`
3. `function deleteProvider(id)` 中：`root.syncWorkingValue()` → `Qt.callLater(root.syncWorkingValue)`
4. `function moveProvider(id, offset)` 中：`syncWorkingValue()` → `Qt.callLater(root.syncWorkingValue)`
5. `providerEnabledSwitch.onToggled` 中：`root.syncWorkingValue()` → `Qt.callLater(root.syncWorkingValue)`

具体 `Edit` 操作示例（重复 5 次，每次改对应行）：

- `providersModel.append(definitionRow(copy(candidate))); root.syncWorkingValue()` → `providersModel.append(definitionRow(copy(candidate))); Qt.callLater(root.syncWorkingValue)`

- `providersModel.set(index, definitionRow(copy(candidate))); editingId = candidate.id; root.syncWorkingValue()` → `providersModel.set(index, definitionRow(copy(candidate))); editingId = candidate.id; Qt.callLater(root.syncWorkingValue)`

- `providersModel.remove(index); root.syncWorkingValue()` → `providersModel.remove(index); Qt.callLater(root.syncWorkingValue)`

- `providersModel.move(index, destination, 1); syncWorkingValue()` → `providersModel.move(index, destination, 1); Qt.callLater(root.syncWorkingValue)`

- `providersModel.set(index, definitionRow(def)); root.syncWorkingValue()` → `providersModel.set(index, definitionRow(def)); Qt.callLater(root.syncWorkingValue)`

- [ ] **Step 2: 同步到已安装目录**

```bash
yes | cp -rf package/. ~/.local/share/plasma/plasmoids/AIQuotaPilot/
systemctl --user restart plasma-plasmashell.service
sleep 4
```

- [ ] **Step 3: 验证（手测，不写自动化）**

```bash
plasmawindowed -a org.kde.plasma.AIQuotaPilot &
sleep 3
```

肉眼检查：右键小组件 → 配置 → 切到"供应商"页 → toggle 任意 Switch → 看右下"应用"按钮是否亮。

- [ ] **Step 4: Commit**

```bash
git add package/contents/ui/config/ProvidersConfig.qml
git commit -m "fix(providers-config): defer syncWorkingValue via Qt.callLater to avoid ListModel.set race"
```

---

## Task 4: 删除空 `onCfg_providersChanged` 函数

**Files:**
- Modify: `package/contents/ui/config/ProvidersConfig.qml:369-371`

- [ ] **Step 1: 编辑**

删除第 369-371 行的：

```qml
onCfg_providersChanged: {
    // 变更已写入 cfg_providers，KCM 框架会自动激活"应用"按钮
}
```

- [ ] **Step 2: 同步并 Commit**

```bash
yes | cp -rf package/. ~/.local/share/plasma/plasmoids/AIQuotaPilot/
git add package/contents/ui/config/ProvidersConfig.qml
git commit -m "chore(providers-config): remove empty onCfg_providersChanged handler"
```

---

## Task 5: Logo `Image.source` 补全 catalogId 降级

**Files:**
- Modify: `package/contents/ui/config/ProviderEditor.qml:259-278`

**Interfaces:**
- 消费：当前 `logoImage.source` 计算
- 产出：catalogId 为空时仍走 SVG 降级；自定义走 logoPath；皆空显示首字符

- [ ] **Step 1: 编辑 ProviderEditor.qml**

将第 264-274 行的 `source:` binding：

```qml
source: {
    if (root.candidate.logoPath && root.candidate.logoPath.length > 0)
        return root.candidate.logoPath
    if (!root.isCustom) {
        const svg = ProviderRegistry.logoSvgFor(
            root.candidate.catalogId || "")
        if (svg && svg.length > 0)
            return "data:image/svg+xml;utf8," + svg
    }
    return ""
}
```

改为：

```qml
source: {
    const catalogId = root.candidate.catalogId || ""
    if (root.candidate.logoPath && root.candidate.logoPath.length > 0)
        return root.candidate.logoPath
    if (catalogId.length > 0 && !root.isCustom) {
        const svg = ProviderRegistry.logoSvgFor(catalogId)
        if (svg && svg.length > 0)
            return "data:image/svg+xml;utf8," + svg
    }
    return ""
}
```

（关键改动：把 `!root.isCustom` 判断改用 catalogId 非空检查，因为 `isCustom` 计算依赖 `catalogId || "custom" === "custom"`，对空 catalogId 仍视为 custom，导致 `ProviderRegistry.logoSvgFor("")` 永不调用。）

- [ ] **Step 2: 同步并 Commit**

```bash
yes | cp -rf package/. ~/.local/share/plasma/plasmoids/AIQuotaPilot/
git add package/contents/ui/config/ProviderEditor.qml
git commit -m "fix(provider-editor): show built-in logo when candidate.catalogId is empty"
```

---

## Task 6: 综合验收

**Files:**
- 无文件改动

- [ ] **Step 1: 清 QML 缓存**

```bash
rm -rf ~/.cache/plasmawindowed/qmlcache/ ~/.cache/plasmashell/qmlcache/
systemctl --user restart plasma-plasmashell.service
sleep 5
```

- [ ] **Step 2: 启动 plasmawindowed 并肉眼验收**

```bash
plasmawindowed -a org.kde.plasma.AIQuotaPilot &
sleep 3
```

依次验证：

1. **改名生效**：右键小组件 → 配置 → 标题"KDE 钱包"或"系统设置"显示的页面名是 `AIQuotaPilot` 或 `额度领航员`（不是 aiUsageWatcher）。
2. **Logo 显示**：右键 → 配置 → "供应商" → 编辑任一内置供应商（如 Codex）→ 顶部圆形 Logo 可见（SVG 或首字符）。
3. **Apply 按钮**：编辑页 → 切换"启用"Switch → 返回列表页 → "应用"按钮亮。
4. **上移下移**：列表页"常规" → 排序模式选"自定义顺序" → 回到供应商页 → 上移/下移按钮可点击 → 操作 → "应用"按钮亮。
5. **删除**：列表页删除按钮 → 确认 → "应用"按钮亮。
6. **持久化**：点击"应用" → 关闭对话框 → 重新打开 → 变更仍在。

- [ ] **Step 3: 验收失败时的回退**

若 Apply 按钮仍不亮，按设计文档 §6 的回退方案处理：用 `KCM.SettingStateBinding` 把 `cfg_providers` 的"非默认高亮"作旁路标记，作为 follow-up 提交。

- [ ] **Step 4: 文档收尾**

更新 `docs/requirements.md` 中"包安装路径"为新 Id。

```bash
git add docs/requirements.md
git commit -m "docs: update install path to AIQuotaPilot Id"
```

---

## Self-Review

1. **Spec coverage**：
   - §4.1 改 Id → Task 1 ✓
   - §4.2 Logo → Task 5 ✓
   - §4.3 Apply 按钮（4 个子步骤）→ Task 2 + 3 + 4 ✓
   - §4.4 紧急修补（cfg_customOrder）→ 已在前置会话完成，不在本计划
   - §4.5 文档与变更 → Task 1 + Task 6 Step 4 ✓
   - §5 验收标准 → Task 6 ✓

2. **Placeholder scan**：
   - 无 TBD/TODO ✓
   - 每个 step 都给了具体代码或命令 ✓
   - 没有 "implement later" 或 "fill in details" ✓

3. **Type consistency**：
   - `syncWorkingValue()` 签名保持一致 ✓
   - `Qt.callLater` 在多个位置使用方式一致 ✓
   - `cfg_providers` 类型保持 string ✓