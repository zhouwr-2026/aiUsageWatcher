import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents
import "../js/mockData.js" as MockData

Item {
    id: root

    property var providers: []
    property int opacityPercent: 80
    property bool keepPanelOpen: false
    property date lastRefreshTime: new Date()

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

    signal refreshRequested()
    signal configureRequested()
    signal keepOpenChanged(bool keepOpen)

    Layout.minimumWidth: Kirigami.Units.gridUnit * 16
    Layout.minimumHeight: Kirigami.Units.gridUnit * 12
    Layout.preferredWidth: Kirigami.Units.gridUnit * 24
    Layout.preferredHeight: Kirigami.Units.gridUnit * 24

    Rectangle {
        anchors.fill: parent
        radius: Kirigami.Units.cornerRadius
        color: Kirigami.Theme.backgroundColor
        opacity: Math.max(20, Math.min(100, root.opacityPercent)) / 100
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Heading {
                id: headerTitle
                objectName: "headerTitle"

                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: qsTr("AI 用量监控")
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
                        root.lastRefreshTime = new Date()
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
            }
        }

        Kirigami.Separator {
            Layout.fillWidth: true
        }

        ListView {
            id: providerList
            objectName: "providerList"

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            cacheBuffer: Kirigami.Units.gridUnit * 100
            model: root.providers
            spacing: Kirigami.Units.smallSpacing

            delegate: ProviderGroup {
                required property var modelData

                objectName: "providerGroup"
                width: ListView.view.width
                providerName: MockData.stripProviderSuffix(modelData.providerName || "")
                ledClass: modelData.ledClass || "led-gray"
                sourceLabel: modelData.sourceLabel || ""
                statusLabel: modelData.statusLabel || ""
                plans: modelData.plans || []
                errorText: modelData.errorText || ""
                templateText: modelData.template || ""
            }
        }

        PlasmaComponents.Label {
            visible: providerList.count === 0
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
            objectName: "statusLabel"

            Layout.fillWidth: true
            text: root.statusText
            color: Kirigami.Theme.disabledTextColor
            font: Kirigami.Theme.smallFont
            elide: Text.ElideRight
        }
    }
}
