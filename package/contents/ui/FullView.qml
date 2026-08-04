import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents
import "../js/providerNormalize.js" as ProviderNormalize
import "../js/displayProvider.js" as DisplayProvider

Item {
    id: root

    property var providers: []
    property int opacityPercent: 80
    property bool keepPanelOpen: false
    property string panelStyle: "bar"
    property date lastRefreshTime: new Date()
    property string sortMode: "default"
    // 手动面板高度（0 = 用默认固定高度）；点击自适应按钮时按内容高度计算后写入
    property real customPanelHeight: 0

    readonly property int renderedPlanCount: {
        let count = 0
        for (let i = 0; i < providers.length; ++i)
            count += Array.isArray(providers[i].plans) ? providers[i].plans.length : 0
        return count
    }
    readonly property int validPlanCount: {
        let count = 0
        for (let i = 0; i < providers.length; ++i) {
            const plans = Array.isArray(providers[i].plans) ? providers[i].plans : []
            for (let j = 0; j < plans.length; ++j) {
                if (plans[j].usedPercent >= 0)
                    ++count
            }
        }
        return count
    }
    readonly property int errorProviderCount: {
        let count = 0
        for (let i = 0; i < providers.length; ++i) {
            if (providers[i].errorText)
                ++count
        }
        return count
    }
    readonly property string statusText: {
        const refreshed = Qt.formatTime(lastRefreshTime, "HH:mm:ss")
        if (providers.length === 0)
            return qsTr("暂无供应商数据 · 最近刷新 %1").arg(refreshed)
        if (validPlanCount === 0)
            return qsTr("暂无有效套餐 · %1 个供应商 · 最近刷新 %2")
                    .arg(providers.length).arg(refreshed)
        const status = qsTr("最近刷新 %1 · %2 个供应商 · %3 个有效套餐")
                .arg(refreshed).arg(providers.length).arg(validPlanCount)
        return errorProviderCount > 0
                ? status + qsTr(" · %1 个异常供应商").arg(errorProviderCount)
                : status
    }
    readonly property string totalPriceText: {
        const total = DisplayProvider.totalPrice(root.providers)
        return total > 0 ? "总价 ¥" + total.toFixed(2) : ""
    }

    function sortModeText() {
        switch (root.sortMode) {
        case "alphabetical": return qsTr("字母 A-Z")
        case "usedPercent": return qsTr("已用%")
        case "remainingPercent": return qsTr("剩余%")
        case "nextReset": return qsTr("最近重置")
        case "custom": return qsTr("自定义")
        default: return qsTr("默认")
        }
    }

    signal refreshRequested()
    signal configureRequested()
    signal keepOpenChanged(bool keepOpen)
    signal heightRequested(real height)
    signal closeRequested()
    signal sortModeRequested(string mode)

    Layout.minimumWidth: Kirigami.Units.gridUnit * 16
    Layout.minimumHeight: Kirigami.Units.gridUnit * 12
    Layout.preferredWidth: Kirigami.Units.gridUnit * 24
    // 默认固定高度；点击自适应按钮后 customPanelHeight 按内容高度计算
    // implicitHeight 与 preferredHeight 同时设置，确保 Plasma popup 外壳响应尺寸变化
    implicitHeight: root.customPanelHeight > 0
        ? root.customPanelHeight
        : Kirigami.Units.gridUnit * 24
    Layout.preferredHeight: implicitHeight

    Rectangle {
        anchors.fill: parent
        radius: Kirigami.Units.cornerRadius
        color: Kirigami.Theme.backgroundColor
        opacity: Math.max(20, Math.min(100, root.opacityPercent)) / 100
    }

    ColumnLayout {
        id: mainColumn

        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            id: headerRow

            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Heading {
                id: headerTitle
                objectName: "headerTitle"

                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: qsTr("额度领航员")
                level: 4
                elide: Text.ElideRight
            }

            RowLayout {
                id: headerActions
                objectName: "headerActions"

                Layout.minimumWidth: implicitWidth
                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents.ToolButton {
                    id: refreshButton
                    objectName: "refreshButton"

                    focusPolicy: Qt.StrongFocus
                    icon.name: "view-refresh"
                    Accessible.name: qsTr("刷新")
                    PlasmaComponents.ToolTip.text: qsTr("刷新")
                    onClicked: {
                        refreshAnimation.restart()
                        root.refreshRequested()
                    }

                    RotationAnimator {
                        id: refreshAnimation
                        target: refreshButton.contentItem
                        from: 0
                        to: 360
                        duration: 300
                    }
                }

                PlasmaComponents.ToolButton {
                    id: autoFitButton
                    objectName: "autoFitButton"

                    focusPolicy: Qt.StrongFocus
                    icon.name: "view-fullscreen"
                    Accessible.name: qsTr("面板高度自适应内容")
                    PlasmaComponents.ToolTip.text: qsTr("面板高度自适应内容")
                    // 只改变面板高度，不改内部布局：按内容高度计算并设置 customPanelHeight
                    onClicked: {
                        const spacing = Kirigami.Units.smallSpacing
                        const content = root.panelStyle === "pie"
                            ? pieContent.contentHeight
                            : providerList.contentHeight
                        let total = headerRow.implicitHeight + content
                        if (root.providers.length === 0)
                            total += Kirigami.Units.gridUnit * 2
                        total += totalPriceLabel.visible ? totalPriceLabel.implicitHeight : 0
                        total += statusLabel.implicitHeight
                        total += usageLegendLabel.implicitHeight
                        total += spacing * 6
                        total += Kirigami.Units.largeSpacing * 2
                        // 安全余量：分隔线高度 + 内容高度估算偏差，避免出现滚动条
                        total += Kirigami.Units.gridUnit * 2
                        root.customPanelHeight = Math.max(total, Kirigami.Units.gridUnit * 6)
                        console.log("[AUTOFIT] content=", content,
                                    "header=", headerRow.implicitHeight,
                                    "total=", total,
                                    "customPanelHeight=", root.customPanelHeight)
                        root.heightRequested(root.customPanelHeight)
                    }
                }

                PlasmaComponents.ToolButton {
                    id: sortButton
                    objectName: "sortButton"

                    property var sortModes: [
                        "default", "alphabetical", "usedPercent",
                        "remainingPercent", "nextReset", "custom"
                    ]

                    focusPolicy: Qt.StrongFocus
                    icon.name: "view-sort"
                    Accessible.name: qsTr("排序：%1").arg(root.sortModeText())
                    PlasmaComponents.ToolTip.text: Accessible.name
                    PlasmaComponents.ToolTip.visible: hovered
                    onClicked: {
                        const currentIndex = sortModes.indexOf(root.sortMode)
                        const nextIndex = (currentIndex + 1) % sortModes.length
                        const nextMode = sortModes[nextIndex]
                        root.sortModeRequested(nextMode)
                    }
                }

                PlasmaComponents.ToolButton {
                    objectName: "configureButton"

                    focusPolicy: Qt.StrongFocus
                    icon.name: "configure"
                    Accessible.name: qsTr("配置")
                    PlasmaComponents.ToolTip.text: qsTr("配置")
                    onClicked: root.configureRequested()
                }

                PlasmaComponents.ToolButton {
                    objectName: "keepOpenButton"

                    focusPolicy: Qt.StrongFocus
                    icon.name: "window-pin"
                    checkable: true
                    checked: root.keepPanelOpen
                    Accessible.name: qsTr("保持面板打开")
                    PlasmaComponents.ToolTip.text: qsTr("保持面板打开")
                    onToggled: root.keepOpenChanged(checked)
                }

                PlasmaComponents.ToolButton {
                    objectName: "closeButton"

                    focusPolicy: Qt.StrongFocus
                    icon.name: "window-close"
                    Accessible.name: qsTr("关闭")
                    PlasmaComponents.ToolTip.text: qsTr("关闭")
                    onClicked: root.closeRequested()
                }
            }
        }

        Kirigami.Separator {
            Layout.fillWidth: true
        }

        ScrollView {
            id: providerScroll
            objectName: "providerScroll"

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            visible: root.panelStyle === "bar"

            ListView {
                id: providerList
                objectName: "providerList"

                width: providerScroll.width
                cacheBuffer: Kirigami.Units.gridUnit * 100
                model: root.providers
                spacing: Kirigami.Units.smallSpacing
                interactive: false

                delegate: ProviderGroup {
                    required property var modelData

                    objectName: "providerGroup"
                    width: ListView.view.width
                    providerName: ProviderNormalize.stripProviderSuffix(modelData.providerName || "")
                    website: modelData.website || ""
                    logoSource: modelData.logoSource || ""
                    logoChar: modelData.logoChar || ""
                    logoIsSvg: modelData.logoIsSvg === true
                    logoBackdropColor: modelData.catalogId === "opencode-go"
                        ? "white" : ""
                    ledClass: modelData.ledClass || "led-gray"
                    sourceLabel: modelData.sourceLabel || ""
                    statusLabel: modelData.statusLabel || ""
                    plans: modelData.plans || []
                    errorText: modelData.errorText || ""
                    templateText: modelData.template || ""
                    priceText: (typeof modelData.price === "number" && modelData.price > 0)
                        ? "¥" + modelData.price.toFixed(2) : ""
                }
            }
        }

        ScrollView {
            id: pieScroll
            objectName: "pieScroll"

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            visible: root.panelStyle === "pie"

            PanelPieView {
                id: pieContent
                objectName: "panelPieView"
                width: pieScroll.width
                providers: root.providers
            }
        }

        PlasmaComponents.Label {
            visible: root.providers.length === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: qsTr("暂无供应商数据")
            color: Kirigami.Theme.disabledTextColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        Kirigami.Separator {
            Layout.fillWidth: true
        }

        PlasmaComponents.Label {
            id: totalPriceLabel

            objectName: "totalPriceLabel"

            Layout.fillWidth: true
            visible: root.totalPriceText.length > 0
            text: root.totalPriceText
            color: Kirigami.Theme.textColor          // 中性色，与状态色语义隔离
            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
            font.bold: true
            elide: Text.ElideRight
        }

        PlasmaComponents.Label {
            id: statusLabel

            objectName: "statusLabel"

            Layout.fillWidth: true
            text: root.statusText + qsTr(" · 排序：%1").arg(root.sortModeText())
            color: Kirigami.Theme.disabledTextColor
            font: Kirigami.Theme.smallFont
            elide: Text.ElideRight
        }

        PlasmaComponents.Label {
            id: usageLegendLabel

            objectName: "usageLegendLabel"

            Layout.fillWidth: true
            text: qsTr("图表说明：高亮为已使用，灰色为剩余额度")
            color: Kirigami.Theme.disabledTextColor
            font: Kirigami.Theme.smallFont
            elide: Text.ElideRight
        }
    }
}
