import QtQuick
import org.kde.plasma.core as PlasmaCore

// 纯 QML 实现的饼图,使用 PlasmaCore.Theme 主题色。
// 数据: [{ label: string, value: number, color: color? }]
// 当传入 [a, b] 两段时,绘制为环形(中心挖空),a 用主色,b 用弱化主色。
Item {
    id: root
    property var data: []
    property color ringColor: PlasmaCore.Theme.HighlightColor
    property color remainingColor: Qt.rgba(1, 1, 1, 0.12)
    property real ringWidthRatio: 0.28  // 环宽占半径比例

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true
        onPaint: {
            const ctx = canvas.getContext("2d");
            ctx.reset();
            const cx = width / 2;
            const cy = height / 2;
            const radius = Math.min(cx, cy) - 2;
            const inner = radius * (1 - root.ringWidthRatio);

            let total = 0;
            for (const d of root.data) total += Math.max(0, d.value || 0);
            if (total <= 0) {
                // 灰环:全部未用
                ctx.beginPath();
                ctx.arc(cx, cy, radius, 0, Math.PI * 2);
                ctx.arc(cx, cy, inner, 0, Math.PI * 2, true);
                ctx.fillStyle = root.remainingColor;
                ctx.fill("evenodd");
                return;
            }

            let start = -Math.PI / 2;
            const colors = [root.ringColor, root.remainingColor, root.ringColor, root.remainingColor];
            for (let i = 0; i < root.data.length; ++i) {
                const v = Math.max(0, root.data[i].value || 0);
                const span = (v / total) * Math.PI * 2;
                ctx.beginPath();
                ctx.moveTo(cx + inner * Math.cos(start), cy + inner * Math.sin(start));
                ctx.arc(cx, cy, radius, start, start + span);
                ctx.arc(cx, cy, inner, start + span, start, true);
                ctx.closePath();
                ctx.fillStyle = (root.data[i].color !== undefined) ? root.data[i].color : colors[i % colors.length];
                ctx.fill();
                start += span;
            }
        }
    }

    onDataChanged: canvas.requestPaint()
    onRingColorChanged: canvas.requestPaint()
    onRemainingColorChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()
}