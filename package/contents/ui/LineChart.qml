import QtQuick
import org.kde.plasma.core as PlasmaCore

// 纯 QML 实现的折线图,使用 PlasmaCore.Theme 主题色。
// values: number[] 历史数值序列,0..100 量纲
Item {
    id: root
    property var values: []
    property color lineColor: PlasmaCore.Theme.HighlightColor
    property real lineWidth: 2
    property real fillOpacity: 0.25

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true
        onPaint: {
            const ctx = canvas.getContext("2d");
            ctx.reset();
            const w = width;
            const h = height;
            const vs = root.values;
            if (!vs || vs.length < 2) {
                // 无数据,提示
                return;
            }
            const stepX = w / (vs.length - 1);
            const minY = 0;
            const maxY = 100;

            // 填充区
            ctx.beginPath();
            ctx.moveTo(0, h);
            for (let i = 0; i < vs.length; ++i) {
                const x = i * stepX;
                const y = h - (Math.max(minY, Math.min(maxY, vs[i])) - minY) / (maxY - minY) * h;
                ctx.lineTo(x, y);
            }
            ctx.lineTo(w, h);
            ctx.closePath();
            ctx.fillStyle = Qt.rgba(root.lineColor.r, root.lineColor.g, root.lineColor.b, root.fillOpacity);
            ctx.fill();

            // 折线
            ctx.beginPath();
            for (let i = 0; i < vs.length; ++i) {
                const x = i * stepX;
                const y = h - (Math.max(minY, Math.min(maxY, vs[i])) - minY) / (maxY - minY) * h;
                if (i === 0) ctx.moveTo(x, y);
                else ctx.lineTo(x, y);
            }
            ctx.lineWidth = root.lineWidth;
            ctx.strokeStyle = root.lineColor;
            ctx.stroke();
        }
    }

    onValuesChanged: canvas.requestPaint()
    onLineColorChanged: canvas.requestPaint()
    onLineWidthChanged: canvas.requestPaint()
    onFillOpacityChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()
}