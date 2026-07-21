import QtQuick
import org.kde.plasma.configuration

ConfigModel {
    ConfigCategory {
        name: qsTr("General")
        icon: "configure"
        source: "config/GeneralConfig.qml"
    }

    ConfigCategory {
        name: qsTr("Providers")
        icon: "network-server"
        source: "config/ProvidersConfig.qml"
    }
}
