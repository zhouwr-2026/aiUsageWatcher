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
QT_QPA_PLATFORM=offscreen "$qmltestrunner" \
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
if plugin["Id"] != "aiUsageWatcher":
    raise SystemExit("KPlugin.Id 必须是 aiUsageWatcher")
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

echo "[static] PASS"
