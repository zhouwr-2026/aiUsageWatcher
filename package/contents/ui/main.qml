import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import "js/mockData.js" as MockData

PlasmoidItem {
    id: root

    // 示例数据；接 KWallet / 用量查询后由 backend 替换
    property var providers: MockData.SEED_PROVIDERS

    function tightestUsedPercent() {
        let worst = -1;
        for (const p of providers) {
            for (const plan of p.plans) {
                if (typeof plan.usedPercent === "number" && plan.usedPercent > worst)
                    worst = plan.usedPercent;

            }
        }
        return worst;
    }

    function tightestProviderName() {
        let worst = -1;
        let name = "";
        for (const p of providers) {
            for (const plan of p.plans) {
                if (typeof plan.usedPercent === "number" && plan.usedPercent > worst) {
                    worst = plan.usedPercent;
                    name = p.providerName;
                }
            }
        }
        return MockData.stripProviderSuffix(name);
    }

    // 定时刷新：每 60 秒用波动数据更新 providers
    Timer {
        interval: 60000
        running: true
        repeat: true
        onTriggered: root.providers = MockData.fluctuateProviders(root.providers)
    }

    // 工具栏操作
    function openConfig() {
        plasmoid.action("configure").trigger();
    }

    function togglePin() {
        // TODO: 实际切换时调 plasmoid.expanded ? alwaysOnTop : off
        // 暂以文本回显作占位
        pinLabel.text = pinLabel.text === i18n("固定") ? i18n("已固定") : i18n("固定");
    }

    // ── compact：圆球显示最紧张的"已用 %" ──────────────────────────
    Plasmoid.compactRepresentation: MouseArea {
        id: compactRoot

        Layout.minimumWidth: PlasmaCore.Units.gridUnit * 2
        Layout.minimumHeight: PlasmaCore.Units.gridUnit * 2
        Layout.preferredWidth: PlasmaCore.Units.gridUnit * 4
        Layout.preferredHeight: PlasmaCore.Units.gridUnit * 4
        onClicked: plasmoid.expanded = !plasmoid.expanded

        Orb {
            anchors.fill: parent
            usedPercent: root.tightestUsedPercent() >= 0 ? root.tightestUsedPercent() : 0
            usedPercentLabel: root.tightestUsedPercent() >= 0 ? Math.round(root.tightestUsedPercent()) + "%" : "—"
            ringClass: {
                const p = root.tightestUsedPercent();
                if (p < 0)
                    return "orb-gray";

                if (p <= 5)
                    return "orb-red";

                if (p <= 15)
                    return "orb-yellow";

                return "orb-green";
            }
            providerName: root.tightestProviderName()
        }

    }

    // ── full：工具栏 + 供应商分组列表 ───────────────────────────────
    Plasmoid.fullRepresentation: Item {
        id: fullRoot

        Layout.minimumWidth: PlasmaCore.Units.gridUnit * 30
        Layout.minimumHeight: PlasmaCore.Units.gridUnit * 18
        Layout.preferredWidth: PlasmaCore.Units.gridUnit * 40
        Layout.preferredHeight: PlasmaCore.Units.gridUnit * 28

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: PlasmaCore.Units.smallSpacing
            spacing: PlasmaCore.Units.smallSpacing

            // 顶部工具栏
            RowLayout {
                Layout.fillWidth: true
                spacing: PlasmaCore.Units.smallSpacing

                PlasmaComponents.Label {
                    text: i18n("模型用量")
                    color: "#f1f5f9"
                    font.bold: true
                    font.pixelSize: PlasmaCore.Units.gridUnit * 0.85
                    Layout.fillWidth: true
                }

                PlasmaComponents.ToolButton {
                    text: i18n("配置")
                    onClicked: root.openConfig()
                }

                PlasmaComponents.ToolButton {
                    id: pinLabel

                    text: i18n("固定")
                    onClicked: root.togglePin()
                }

            }

            // 分组列表（每个供应商一段；不够位置时滚动）
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ColumnLayout {
                    width: fullRoot.width - 2 * PlasmaCore.Units.smallSpacing
                    spacing: PlasmaCore.Units.smallSpacing

                    Repeater {
                        model: root.providers

                        delegate: ProviderGroup {
                            Layout.fillWidth: true
                            providerName: modelData.providerName
                            ledClass: modelData.ledClass
                            sourceLabel: modelData.sourceLabel
                            statusLabel: modelData.statusLabel
                            errorText: modelData.errorText
                            plans: modelData.plans
                        }

                    }

                    // 没有供应商时的占位
                    PlasmaComponents.Label {
                        visible: root.providers.length === 0
                        text: i18n("暂无供应商。请在「配置」中添加。")
                        color: "#94a3b8"
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        font.pixelSize: PlasmaCore.Units.gridUnit * 0.7
                    }

                }

            }

        }

    }

}
