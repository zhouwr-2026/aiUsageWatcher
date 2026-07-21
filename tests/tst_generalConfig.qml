import QtQuick
import QtTest

Item {
    id: host

    width: 480
    height: 480

    readonly property url configModelUrl: Qt.resolvedUrl("../package/contents/config/config.qml")
    readonly property url generalConfigUrl: Qt.resolvedUrl("../package/contents/ui/config/GeneralConfig.qml")
    readonly property url providersConfigUrl: Qt.resolvedUrl("../package/contents/ui/config/ProvidersConfig.qml")

    TestCase {
        name: "GeneralConfig"
        when: windowShown

        function createFrom(url, properties) {
            const component = Qt.createComponent(url)
            compare(component.status, Component.Ready, component.errorString())
            const object = component.createObject(host, properties || {})
            verify(object !== null, component.errorString())
            return object
        }

        function test_standard_config_model_has_general_and_providers() {
            const model = createFrom(host.configModelUrl)
            compare(model.count, 2)
            compare(model.get(0).name, "General")
            compare(model.get(0).source, "config/GeneralConfig.qml")
            compare(model.get(1).name, "Providers")
            compare(model.get(1).source, "config/ProvidersConfig.qml")
            model.destroy()
        }

        function test_cfg_values_initialize_controls() {
            const page = createFrom(host.generalConfigUrl, {
                cfg_compactStyle: "bar",
                cfg_refreshIntervalSec: 120,
                cfg_opacityPercent: 65,
                cfg_keepPanelOpen: true
            })

            compare(findChild(page, "compactStyleControl").currentValue, "bar")
            compare(findChild(page, "refreshIntervalControl").value, 120)
            compare(findChild(page, "opacityControl").value, 65)
            compare(findChild(page, "keepPanelOpenControl").checked, true)
            page.destroy()
        }

        function test_controls_update_cfg_values() {
            const page = createFrom(host.generalConfigUrl, {
                cfg_compactStyle: "pie",
                cfg_refreshIntervalSec: 60,
                cfg_opacityPercent: 80,
                cfg_keepPanelOpen: false
            })
            const compactStyle = findChild(page, "compactStyleControl")
            const refreshInterval = findChild(page, "refreshIntervalControl")
            const opacity = findChild(page, "opacityControl")
            const keepPanelOpen = findChild(page, "keepPanelOpenControl")

            compactStyle.currentIndex = compactStyle.indexOfValue("bar")
            compactStyle.activated(compactStyle.currentIndex)
            refreshInterval.value = 300
            opacity.value = 55
            keepPanelOpen.checked = true

            compare(page.cfg_compactStyle, "bar")
            compare(page.cfg_refreshIntervalSec, 300)
            compare(page.cfg_opacityPercent, 55)
            compare(page.cfg_keepPanelOpen, true)
            page.destroy()
        }

        function test_both_pages_accept_plasma_full_config_injection() {
            const properties = {
                cfg_providers: "[]",
                cfg_providersDefault: "",
                cfg_compactStyle: "pie",
                cfg_compactStyleDefault: "pie",
                cfg_refreshIntervalSec: 60,
                cfg_refreshIntervalSecDefault: 60,
                cfg_opacityPercent: 80,
                cfg_opacityPercentDefault: 80,
                cfg_keepPanelOpen: false,
                cfg_keepPanelOpenDefault: false
            }
            const generalPage = createFrom(host.generalConfigUrl, properties)
            const providersPage = createFrom(host.providersConfigUrl, properties)

            compare(generalPage.cfg_providers, "[]")
            compare(providersPage.cfg_compactStyle, "pie")
            generalPage.destroy()
            providersPage.destroy()
        }
    }
}
