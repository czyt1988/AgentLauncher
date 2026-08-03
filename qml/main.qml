import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 960
    height: 640
    minimumWidth: 720
    minimumHeight: 480
    visible: true
    title: qsTr("AgentLauncher")

    color: "#1e1e2e"

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: homePage
    }

    // Home: title + responsive grid of agent cards.
    Component {
        id: homePage

        ScrollView {
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.availableWidth
                spacing: 8

                Label {
                    text: qsTr("Agent Launcher")
                    color: "#cdd6f4"
                    font.pixelSize: 26
                    font.bold: true
                    Layout.leftMargin: 24
                    Layout.topMargin: 24
                    Layout.bottomMargin: 8
                }

                Label {
                    text: qsTr("Launch AI coding agents and open their web UI")
                    color: "#7f849c"
                    font.pixelSize: 13
                    Layout.leftMargin: 24
                    Layout.bottomMargin: 8
                }

                Flow {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    Layout.bottomMargin: 24
                    spacing: 20

                    Repeater {
                        model: agentModel
                        delegate: AgentCard {
                            width: 260
                            onConfigureRequested: function(id) {
                                stack.push(configPageComp, { "agentId": id });
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: configPageComp
        ConfigPage {}
    }

    // Central error display for launch/stop failures. The matching card also
    // flashes red (see AgentCard.qml) for at-place feedback.
    Popup {
        id: errorPopup
        property string message: ""
        anchors.centerIn: parent
        modal: true
        focus: true
        width: 420
        padding: 20
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: "#1e1e2e"
            border.color: "#f38ba8"
            border.width: 1
            radius: 12
        }

        ColumnLayout {
            width: errorPopup.availableWidth
            spacing: 14

            Label {
                text: qsTr("Launch failed")
                color: "#f38ba8"
                font.pixelSize: 16
                font.bold: true
            }
            Label {
                Layout.fillWidth: true
                text: errorPopup.message
                color: "#cdd6f4"
                font.pixelSize: 13
                wrapMode: Text.Wrap
            }
            Button {
                Layout.alignment: Qt.AlignRight
                text: qsTr("OK")
                background: Rectangle { radius: 8; color: parent.down ? "#45475a" : (parent.hovered ? "#4a4d62" : "#313244"); border.color: "#45475a" }
                contentItem: Label { text: parent.text; color: "#cdd6f4"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                onClicked: errorPopup.close()
            }
        }
    }

    Connections {
        target: launcher
        function onLaunchFailed(id, message) {
            errorPopup.message = message
            errorPopup.open()
        }
    }
}
