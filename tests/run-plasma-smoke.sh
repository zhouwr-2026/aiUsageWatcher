#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

qmltestrunner=/usr/lib/qt6/bin/qmltestrunner
tmp_dir="$(mktemp -d)"
cleanup() {
    rm -rf -- "$tmp_dir"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'exit 129' HUP

for tool in "$qmltestrunner" /usr/bin/cmake /usr/bin/kpackagetool6 /usr/bin/diff /usr/bin/rg /usr/bin/ldd /usr/bin/python3; do
    [[ -x "$tool" ]] || { echo "缺少冒烟工具: $tool" >&2; exit 1; }
done
if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
    echo "BLOCKED: 当前没有可见桌面会话（DISPLAY/WAYLAND_DISPLAY 均为空）" >&2
    exit 2
fi
if [[ -z "${DBUS_SESSION_BUS_ADDRESS:-}" ]]; then
    echo "BLOCKED: 当前没有 Plasma 所需的 D-Bus 会话" >&2
    exit 2
fi

user_install_prefix="${HOME}/.local"
echo "[smoke] 构建并安装 QML 包与 C++ 后端"
/usr/bin/cmake -S . -B "$tmp_dir/build" \
    -DBUILD_TESTING=OFF \
    -DCMAKE_INSTALL_PREFIX="$user_install_prefix"
/usr/bin/cmake --build "$tmp_dir/build" -j2
/usr/bin/cmake --install "$tmp_dir/build"

plugin_path="$user_install_prefix/lib/qt6/plugins/plasma/applets/org.kde.plasma.AIQuotaPilot.so"
worker_path="$user_install_prefix/libexec/quota-pilot-script-worker"
if [[ ! -f "$plugin_path" ]]; then
    echo "原生插件未安装到预期位置: $plugin_path" >&2
    exit 1
fi
if [[ ! -x "$worker_path" ]]; then
    echo "脚本 worker 未安装到预期位置: $worker_path" >&2
    exit 1
fi

package_info="$(/usr/bin/kpackagetool6 --type Plasma/Applet --show org.kde.plasma.AIQuotaPilot)"
installed_dir="$(printf '%s\n' "$package_info" \
    | sed -n \
        -e 's/^[[:space:]]*Path[[:space:]]*:[[:space:]]*//p' \
        -e 's/^[[:space:]]*路径[[:space:]]*：[[:space:]]*//p' \
    | head -n 1)"
installed_dir="${installed_dir%/}"
if [[ -z "$installed_dir" || ! -d "$installed_dir" ]]; then
    printf '%s\n' "$package_info" >&2
    echo "无法从 kpackagetool6 --show 解析安装目录" >&2
    exit 1
fi
if [[ "$(basename -- "$installed_dir")" != "org.kde.plasma.AIQuotaPilot" ]]; then
    echo "安装目录 ID 不一致: $installed_dir" >&2
    exit 1
fi

echo "[smoke] 副本一致性: $installed_dir"
/usr/bin/diff -qr \
    --exclude='.directory' \
    --exclude='*.qmlc' \
    --exclude='*.jsc' \
    package "$installed_dir"

echo "[smoke] 可见组件证据: tst_fullView 断言 3 provider / 6 PlanBar"
/usr/bin/rg -q 'compare\(descendantsNamed\(fullView, "providerGroup"\)\.length, 3\)' \
    tests/tst_fullView.qml
/usr/bin/rg -q 'compare\(descendantsNamed\(fullView, "planBar"\)\.length, 6\)' \
    tests/tst_fullView.qml
QT_QPA_PLATFORM=offscreen "$qmltestrunner" \
    -input tests/tst_fullView.qml \
    -import package/contents/ui

QT_QPA_PLATFORM=offscreen "$qmltestrunner" \
    -input tests/tst_displayProvider.qml \
    -import package/contents/ui

echo "[smoke] 原生插件可加载（dlopen RTLD_NOW 检查未定义符号）"
/usr/bin/python3 - "$plugin_path" <<'PYEOF'
import ctypes, os, sys
try:
    ctypes.CDLL(sys.argv[1], mode=os.RTLD_NOW)
except OSError as e:
    print("插件加载失败:", e)
    sys.exit(1)
PYEOF

echo "[smoke] 原生插件依赖"
if /usr/bin/ldd "$plugin_path" | /usr/bin/rg -n 'not found'; then
    echo "原生插件存在缺失依赖" >&2
    exit 1
fi

echo "[smoke] 独立脚本 worker"
worker_result="$($worker_path <<'EOF'
{"mode":"extract","script":"({request:{url:'https://example.com'},extractor:function(response){return {used:response.used,limit:response.limit}}})","response":{"used":1,"limit":2}}
EOF
)"
if ! /usr/bin/python3 -c 'import json,sys; r=json.load(sys.stdin); assert r["ok"] and r["value"] == {"used":1,"limit":2}' <<<"$worker_result"; then
    echo "脚本 worker 自测失败" >&2
    exit 1
fi

echo "[smoke] PASS（不启动第二个 Plasma 进程；部署后通过用户服务日志验证）"
