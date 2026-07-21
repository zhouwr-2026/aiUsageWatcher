#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

qmltestrunner=/usr/lib/qt6/bin/qmltestrunner
tmp_dir="$(mktemp -d)"
runtime_pid=""

cleanup() {
    if [[ -n "$runtime_pid" ]] && kill -0 "$runtime_pid" 2>/dev/null; then
        kill -TERM "$runtime_pid" 2>/dev/null || true
        wait "$runtime_pid" 2>/dev/null || true
    fi
    rm -rf -- "$tmp_dir"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'exit 129' HUP

for tool in "$qmltestrunner" /usr/bin/cmake /usr/bin/kpackagetool6 /usr/bin/plasmawindowed /usr/bin/diff /usr/bin/rg; do
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

plugin_path="$user_install_prefix/lib/qt6/plugins/plasma/applets/aiUsageWatcher.so"
if [[ ! -f "$plugin_path" ]]; then
    echo "原生插件未安装到预期位置: $plugin_path" >&2
    exit 1
fi

package_info="$(/usr/bin/kpackagetool6 --type Plasma/Applet --show aiUsageWatcher)"
installed_dir="$(printf '%s\n' "$package_info" \
    | sed -n 's/^[[:space:]]*Path[[:space:]]*:[[:space:]]*//p' \
    | head -n 1)"
installed_dir="${installed_dir%/}"
if [[ -z "$installed_dir" || ! -d "$installed_dir" ]]; then
    printf '%s\n' "$package_info" >&2
    echo "无法从 kpackagetool6 --show 解析安装目录" >&2
    exit 1
fi
if [[ "$(basename -- "$installed_dir")" != "aiUsageWatcher" ]]; then
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

runtime_log="$tmp_dir/plasmawindowed.log"
echo "[smoke] 运行: plasmawindowed aiUsageWatcher（8 秒观察窗）"
/usr/bin/env -u MINIMAX_API_KEY \
    QT_FORCE_STDERR_LOGGING=1 \
    QT_PLUGIN_PATH="$user_install_prefix/lib/qt6/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}" \
    /usr/bin/plasmawindowed aiUsageWatcher >"$runtime_log" 2>&1 &
runtime_pid=$!
sleep 8

if ! kill -0 "$runtime_pid" 2>/dev/null; then
    set +e
    wait "$runtime_pid"
    runtime_status=$?
    set -e
    runtime_pid=""
    echo "plasmawindowed 在观察窗内提前退出，status=$runtime_status" >&2
    sed -n '1,240p' "$runtime_log" >&2
    exit 1
fi

kill -TERM "$runtime_pid" 2>/dev/null || true
set +e
wait "$runtime_pid"
runtime_status=$?
set -e
runtime_pid=""
echo "[smoke] plasmawindowed exit status after owned TERM: $runtime_status"
if [[ "$runtime_status" -eq 124 ]]; then
    echo "拒绝把 timeout 124 单独视为成功" >&2
    exit 1
fi
if [[ "$runtime_status" -ne 0 && "$runtime_status" -ne 143 ]]; then
    sed -n '1,240p' "$runtime_log" >&2
    echo "plasmawindowed 非预期退出状态: $runtime_status" >&2
    exit 1
fi

forbidden='ReferenceError|TypeError|PlasmaCore\.Units|Error loading QML file|Failed to load plugin|Could not load plugin'
if /usr/bin/rg -n "$forbidden" "$runtime_log"; then
    echo "运行日志包含禁用错误" >&2
    exit 1
fi

if ! /usr/bin/rg -q 'aiUsageWatcher: native backend loaded' "$runtime_log"; then
    sed -n '1,240p' "$runtime_log" >&2
    echo "未观察到原生后端加载证据" >&2
    exit 1
fi

echo "[smoke] runtime forbidden patterns: 0"
echo "[smoke] PASS（发布前仍须按 tests/README.md 保存 popup/KCM 人工证据）"
