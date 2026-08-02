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
}
