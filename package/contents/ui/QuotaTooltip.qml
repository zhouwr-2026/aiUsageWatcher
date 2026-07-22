pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Item {
    id: root

    property var provider: ({})
    readonly property var plans: Array.isArray(provider.plans) ? provider.plans : []
    readonly property var firstPlan: plans.length > 0 ? plans[0] : null

    implicitWidth: Kirigami.Units.gridUnit * 18
    implicitHeight: content.implicitHeight

    function titleText() {
        const providerName = provider.providerName || qsTr("暂无模型")
        return firstPlan && firstPlan.planName
            ? providerName + " · " + firstPlan.planName : providerName
    }

    function usageText() {
        if (provider.errorText)
            return provider.errorText
        if (!firstPlan)
            return provider.statusLabel || qsTr("暂无数据")
        if (firstPlan.isInvalid)
            return firstPlan.invalidReason || qsTr("暂无数据")

        const used = firstPlan.usedText !== undefined && firstPlan.usedText !== ""
            ? firstPlan.usedText : "—"
        const total = firstPlan.totalText !== undefined && firstPlan.totalText !== ""
            ? firstPlan.totalText : "—"
        const unit = firstPlan.unitText || firstPlan.unitOverflow || ""
        let summary = qsTr("已用：%1/%2").arg(used).arg(total)
        if (unit)
            summary += " " + unit
        if (firstPlan.resetText)
            summary += qsTr(" · 重置于 %1").arg(firstPlan.resetText)
        return summary
    }

    ColumnLayout {
        id: content

        anchors.left: parent.left
        anchors.right: parent.right
        spacing: Kirigami.Units.smallSpacing

        QQC2.Label {
            objectName: "tooltipTitle"
            Layout.fillWidth: true
            text: root.titleText()
            font.bold: true
            elide: Text.ElideRight
        }

        QQC2.Label {
            objectName: "tooltipUsageSummary"
            Layout.fillWidth: true
            text: root.usageText()
            color: root.provider.errorText
                ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
            wrapMode: Text.Wrap
        }
    }
}
