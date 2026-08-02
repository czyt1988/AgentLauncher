import QtQuick
import QtQuick.Controls

Item {
    id: root
    height: 230

    // Alias model roles to distinct local properties (avoids shadowing by
    // Rectangle.color etc.).
    property string agentId_p: agentId
    property string name_p: name
    property string icon_p: icon
    property string agentColor: color
    property bool running_p: running

    signal configureRequested(string id)

    Rectangle {
        id: card
        anchors.fill: parent
        radius: 16

        // Visual state: running => tinted background with colored border.
        color: root.running_p
              ? Qt.rgba(tintRed(root.agentColor), tintGreen(root.agentColor), tintBlue(root.agentColor), 0.16)
              : "#313244"
        border.width: root.running_p ? 2.5 : 1
        border.color: root.running_p ? root.agentColor : "#45475a"
        Behavior on color { ColorAnimation { duration: 180 } }
        Behavior on border.color { ColorAnimation { duration: 180 } }

        // Click the card body: open web UI when running, otherwise launch.
        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (root.running_p)
                    launcher.openWeb(root.agentId_p)
                else
                    launcher.launch(root.agentId_p)
            }
        }

        Column {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 10

            Row {
                spacing: 12
                Layout.alignment: Qt.AlignHCenter

                Image {
                    source: root.icon_p
                    sourceSize.width: 40
                    sourceSize.height: 40
                    width: 40
                    height: 40
                    fillMode: Image.PreserveAspectFit
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 12
                    height: 12
                    radius: 6
                    color: root.running_p ? root.agentColor : "#585b70"
                    Behavior on color { ColorAnimation { duration: 180 } }
                }
            }

            Label {
                text: root.name_p
                color: "#cdd6f4"
                font.pixelSize: 18
                font.bold: true
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                text: root.running_p ? qsTr("Running") : qsTr("Stopped")
                color: root.running_p ? root.agentColor : "#7f849c"
                font.pixelSize: 12
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
            }

            Item { width: 1; height: 4 }

            Row {
                spacing: 10
                width: parent.width

                Button {
                    text: root.running_p ? qsTr("Open") : qsTr("Start")
                    width: (parent.width - 10) / 2

                    background: Rectangle {
                        radius: 8
                        color: parent.down ? Qt.darker(root.agentColor, 1.3)
                                           : (parent.hovered ? Qt.darker(root.agentColor, 1.15) : root.agentColor)
                    }
                    contentItem: Label {
                        text: parent.text
                        color: "#ffffff"
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (root.running_p)
                            launcher.openWeb(root.agentId_p)
                        else
                            launcher.launch(root.agentId_p)
                    }
                }

                Button {
                    text: qsTr("Configure")
                    width: (parent.width - 10) / 2

                    background: Rectangle {
                        radius: 8
                        color: parent.down ? "#45475a"
                                           : (parent.hovered ? "#4a4d62" : "#45475a")
                    }
                    contentItem: Label {
                        text: parent.text
                        color: "#cdd6f4"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: root.configureRequested(root.agentId_p)
                }
            }
        }
    }

    function tintRed(hex) { return parseInt(hex.substring(1, 3), 16) / 255 }
    function tintGreen(hex) { return parseInt(hex.substring(3, 5), 16) / 255 }
    function tintBlue(hex) { return parseInt(hex.substring(5, 7), 16) / 255 }
}
