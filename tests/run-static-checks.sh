#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

qmltestrunner=/usr/lib/qt6/bin/qmltestrunner
qmllint=/usr/lib/qt6/bin/qmllint
tmp_dir="$(mktemp -d)"
trap 'rm -rf -- "$tmp_dir"' EXIT

for tool in "$qmltestrunner" "$qmllint" /usr/bin/xmllint /usr/bin/python3 /usr/bin/rg /usr/bin/kpackagetool6; do
    [[ -x "$tool" ]] || { echo "缺少验证工具: $tool" >&2; exit 1; }
done

echo "[static] qmltestrunner: 全套组件与逻辑测试"
QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=org.kde.desktop "$qmltestrunner" \
    -input tests \
    -import package/contents/ui

echo "[static] qmllint: 全部生产 QML"
"$qmllint" \
    package/contents/ui/*.qml \
    package/contents/ui/config/*.qml \
    package/contents/config/config.qml

echo "[static] xmllint: KConfig XML"
/usr/bin/xmllint --noout package/contents/config/main.xml

echo "[static] metadata: 必填字段与 AppStream 输出"
/usr/bin/python3 - <<'PY'
import json
from pathlib import Path

metadata = json.loads(Path("package/metadata.json").read_text(encoding="utf-8"))
plugin = metadata.get("KPlugin", {})
required = {
    "KPlugin.Name": plugin.get("Name"),
    "KPlugin.License": plugin.get("License"),
    "KPlugin.Id": plugin.get("Id"),
    "KPackageStructure": metadata.get("KPackageStructure"),
    "X-Plasma-API-Minimum-Version": metadata.get("X-Plasma-API-Minimum-Version"),
}
empty = [name for name, value in required.items()
         if not isinstance(value, str) or not value.strip()]
if empty:
    raise SystemExit("metadata 必填字段为空: " + ", ".join(empty))
if plugin["Id"] != "org.kde.plasma.AIQuotaPilot":
    raise SystemExit("KPlugin.Id 必须是 org.kde.plasma.AIQuotaPilot")
if plugin["Name"] != "额度领航员":
    raise SystemExit("KPlugin.Name 必须是额度领航员")
if metadata["KPackageStructure"] != "Plasma/Applet":
    raise SystemExit("KPackageStructure 必须是 Plasma/Applet")
if metadata["X-Plasma-API-Minimum-Version"] != "6.0":
    raise SystemExit("X-Plasma-API-Minimum-Version 必须是 6.0")
PY
/usr/bin/kpackagetool6 --appstream-metainfo package >"$tmp_dir/appstream.xml"
/usr/bin/xmllint --noout "$tmp_dir/appstream.xml"
[[ -n "$(/usr/bin/xmllint --xpath 'string(/component/name)' "$tmp_dir/appstream.xml")" ]]
[[ -n "$(/usr/bin/xmllint --xpath 'string(/component/project_license)' "$tmp_dir/appstream.xml")" ]]

echo "[static] forbidden patterns: Plasma 5 theme API、外部 KCM 声明、硬编码主题色"
if /usr/bin/rg -n \
    'PlasmaCore\.(Units|Theme)|X-KDE-ConfigModule' \
    package/contents package/metadata.json; then
    echo "发现禁用 Plasma/KCM 模式" >&2
    exit 1
fi
if /usr/bin/rg -n '#[0-9A-Fa-f]{6}([0-9A-Fa-f]{2})?' package/contents --glob '*.qml'; then
    echo "发现硬编码十六进制主题色" >&2
    exit 1
fi
echo "[static] native chart gate: 图表必须使用 Plasma 原生 Charts.PieChart / QQC2.ProgressBar"
for chart_file in package/contents/ui/CompactView.qml package/contents/ui/PanelPieView.qml package/contents/ui/PlanBar.qml; do
    if /usr/bin/rg -n 'QtQuick\.Shapes|PathAngleArc|Canvas|ShaderEffect' "$chart_file"; then
        echo "图表禁止自绘路径或着色器: $chart_file" >&2
        exit 1
    fi
done
if ! /usr/bin/rg -q 'Charts\.PieChart' package/contents/ui/CompactView.qml \
   || ! /usr/bin/rg -q 'QQC2\.ProgressBar' package/contents/ui/CompactView.qml; then
    echo "CompactView.qml 必须同时包含 Charts.PieChart 和 QQC2.ProgressBar" >&2
    exit 1
fi
if ! /usr/bin/rg -q 'Charts\.PieChart' package/contents/ui/PanelPieView.qml; then
    echo "PanelPieView.qml 必须包含 Charts.PieChart" >&2
    exit 1
fi
if ! /usr/bin/rg -q 'QQC2\.ProgressBar' package/contents/ui/PlanBar.qml; then
    echo "PlanBar.qml 必须包含 QQC2.ProgressBar" >&2
    exit 1
fi
if /usr/bin/rg -n 'Plasmoid\.contextualActions[[:space:]]*:[[:space:]]*\[[^]]*icon\.name:[[:space:]]*"configure"' \
    package/contents/ui/main.qml --multiline; then
    echo "不要在右键菜单重复添加 Plasma 已提供的配置入口" >&2
    exit 1
fi
if ! /usr/bin/rg -q '^pragma ComponentBehavior: Bound$' package/contents/ui/main.qml; then
    echo "compact/full 组件必须绑定到本小部件上下文，否则面板点击目标可能解析错误" >&2
    exit 1
fi
if ! /usr/bin/rg -q 'activationTogglesExpanded:[[:space:]]*true' package/contents/ui/main.qml; then
    echo "小部件必须允许 Plasma 激活操作切换 popup" >&2
    exit 1
fi

echo "[static] PASS"
