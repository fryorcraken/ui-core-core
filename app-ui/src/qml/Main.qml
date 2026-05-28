import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#1a1a1a"

    readonly property var backend: logos.module("app_ui")

    function mark(b) { return b === undefined ? "?" : (b ? "yes" : "no") }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 12

        Text {
            text: "Logos Status"
            color: "#ffffff"
            font.pixelSize: 22
        }

        Text {
            text: backend ? "Backend ready" : "No backend bridge"
            color: backend ? "#56d364" : "#f0883e"
            font.pixelSize: 12
        }

        Text {
            text: "Storage:  started=" + root.mark(backend ? backend.storageStarted : undefined)
                + "   connected=" + root.mark(backend ? backend.storageConnected : undefined)
            color: "#ffffff"
            font.pixelSize: 16
        }

        Text {
            text: "Delivery: started=" + root.mark(backend ? backend.deliveryStarted : undefined)
                + "   connected=" + root.mark(backend ? backend.deliveryConnected : undefined)
            color: "#ffffff"
            font.pixelSize: 16
        }

        Item { Layout.fillHeight: true }
    }
}
