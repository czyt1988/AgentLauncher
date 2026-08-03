import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page
    property string agentId: ""

    background: Rectangle { color: "#1e1e2e" }

    // Load agent data once on appear. NOTE: do not name this `data` — that
    // collides with QQuickItem's built-in `data` (children group, non-NOTIFY),
    // so child bindings silently resolved to the wrong property and fields
    // came up empty.
    property var agentData: agentModel.agent(agentId)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 16

        RowLayout {
            spacing: 12

            Button {
                text: qsTr("\u2190 Back")
                background: Rectangle { color: "transparent" }
                contentItem: Label { text: parent.text; color: "#89b4fa"; font.pixelSize: 16 }
                onClicked: page.StackView.view.pop()
            }
            Item { Layout.fillWidth: true }
        }

        Label {
            text: agentData.name || qsTr("Configure Agent")
            color: "#cdd6f4"
            font.pixelSize: 24
            font.bold: true
        }

        Label {
            text: qsTr("Startup command")
            color: "#a6adc8"
            font.pixelSize: 13
        }
        TextField {
            id: commandField
            Layout.fillWidth: true
            text: agentData.command || ""
            color: "#cdd6f4"
            placeholderText: qsTr("e.g. kimi web --port 58628")
            background: Rectangle { color: "#313244"; radius: 8; border.color: "#45475a" }
        }

        Label {
            text: qsTr("Web URL")
            color: "#a6adc8"
            font.pixelSize: 13
        }
        TextField {
            id: webUrlField
            Layout.fillWidth: true
            text: agentData.webUrl || ""
            color: "#cdd6f4"
            placeholderText: qsTr("e.g. http://127.0.0.1:58628")
            background: Rectangle { color: "#313244"; radius: 8; border.color: "#45475a" }
        }
        Label {
            text: qsTr("Tip: keep the port in the command and the Web URL in sync.")
            color: "#6c7086"
            font.pixelSize: 11
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        RowLayout {
            spacing: 12

            Label {
                text: qsTr("Config directory:")
                color: "#a6adc8"
                font.pixelSize: 13
            }
            Label {
                id: dirLabel
                text: agentData.configDir || qsTr("(not set)")
                color: "#89b4fa"
                font.pixelSize: 13
                Layout.fillWidth: true
                elide: Text.ElideMiddle
            }
            Button {
                text: qsTr("Open")
                enabled: (agentData.configDir || "").length > 0
                background: Rectangle { radius: 8; color: parent.down ? "#45475a" : (parent.hovered ? "#4a4d62" : "#313244"); border.color: "#45475a" }
                contentItem: Label { text: parent.text; color: "#cdd6f4"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                onClicked: launcher.openConfigDir(page.agentId)
            }
        }

        Item { Layout.fillHeight: true }

        Row {
            spacing: 12
            Layout.alignment: Qt.AlignRight

            Button {
                text: qsTr("Cancel")
                background: Rectangle { radius: 8; color: parent.down ? "#45475a" : "#313244"; border.color: "#45475a" }
                contentItem: Label { text: parent.text; color: "#cdd6f4"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                onClicked: page.StackView.view.pop()
            }
            Button {
                text: qsTr("Save")
                background: Rectangle { radius: 8; color: parent.down ? Qt.darker(agentData.color || "#89b4fa", 1.3) : (agentData.color || "#89b4fa") }
                contentItem: Label { text: parent.text; color: "#ffffff"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                onClicked: {
                    launcher.updateAgent(page.agentId, commandField.text, webUrlField.text)
                    page.StackView.view.pop()
                }
            }
        }
    }
}
