import QtQuick
import org.kde.plasma.configuration

ConfigModel {
    ConfigCategory {
        name: qsTr("常规")
        icon: "configure"
        source: "config/GeneralConfig.qml"
    }

    ConfigCategory {
        name: qsTr("供应商")
        icon: "network-server"
        source: "config/ProvidersConfig.qml"
    }
}
