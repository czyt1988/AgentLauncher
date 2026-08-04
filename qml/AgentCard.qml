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
    property bool launching_p: launching
    property bool installed_p: installed
    property string version_p: version
    property bool installing_p: installing
    property string installCommand_p: installCommand

    signal configureRequested(string id)

    // At-place launch/stop error feedback: briefly tint the border red and
    // show the (elided) reason in the status slot. The full message also pops
    // up centrally (main.qml). We set `flashing` explicitly (not via a binding
    // to Timer.running, which is non-NOTIFYable) so updates actually fire.
    property string flashMessage: ""
    property bool flashing: false
    Timer {
        id: flashTimer
        interval: 4000
        onTriggered: { root.flashing = false; root.flashMessage = "" }
    }
    Connections {
        target: launcher
        function onLaunchFailed(id, message) {
            if (id === root.agentId_p) {
                root.flashMessage = message
                root.flashing = true
                flashTimer.restart()
            }
        }
    }

    // Transient UI state for the stop button: set immediately on click so
    // the card shows "Stopping…" + a spinner before the health check (500ms
    // later) confirms the agent is down. Cleared when running_p goes false.
    property bool stopping: false
    onRunning_pChanged: if (!running_p) stopping = false

    Rectangle {
        id: card
        anchors.fill: parent
        radius: 16

        // Visual state: running => tinted background with colored border.
        color: root.running_p
              ? Qt.rgba(tintRed(root.agentColor), tintGreen(root.agentColor), tintBlue(root.agentColor), 0.16)
              : "#313244"
        border.width: root.running_p ? 2.5 : 1
        border.color: root.flashing ? "#f38ba8"
                                    : (root.running_p ? root.agentColor : "#45475a")
        Behavior on color { ColorAnimation { duration: 180 } }
        Behavior on border.color { ColorAnimation { duration: 180 } }

        // Click the card body: open web UI when running, otherwise launch.
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            onClicked: {
                if (root.running_p)
                    launcher.openWeb(root.agentId_p)
                else if (!root.launching_p)
                    launcher.launch(root.agentId_p)
            }
        }

        // Right-click context menu.
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            onClicked: contextMenu.popup()
        }

        Menu {
            id: contextMenu

            MenuItem {
                text: root.running_p ? qsTr("关闭") : qsTr("开启")
                onTriggered: {
                    if (root.running_p)
                        launcher.stop(root.agentId_p)
                    else
                        launcher.launch(root.agentId_p)
                }
            }
            MenuItem {
                text: root.installed_p ? qsTr("更新") : qsTr("安装")
                enabled: !root.installing_p && root.installCommand_p.length > 0
                onTriggered: {
                    if (root.installed_p)
                        launcher.updateTool(root.agentId_p)
                    else
                        launcher.install(root.agentId_p)
                }
            }
            MenuItem {
                text: qsTr("设置")
                onTriggered: root.configureRequested(root.agentId_p)
            }
            MenuItem {
                text: qsTr("打开配置文件夹")
                onTriggered: launcher.openConfigDir(root.agentId_p)
            }
        }

        // Top-left indicator: version label (installed), download icon (not
        // installed), or spinner (installing). Mirrors the top-right × stop
        // button's positioning.
        Item {
            id: versionIndicator
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.topMargin: 8
            anchors.leftMargin: 8
            width: 60
            height: 22

            // Installing state: spinner
            BusyIndicator {
                visible: root.installing_p
                running: root.installing_p
                width: 16
                height: 16
                anchors.centerIn: parent
            }

            // Not installed: download icon (clickable → install)
            Item {
                visible: !root.installed_p && !root.installing_p
                anchors.fill: parent

                Rectangle {
                    anchors.fill: parent
                    radius: 11
                    color: downloadArea.containsMouse
                           ? Qt.rgba(137/255, 180/255, 250/255, 0.22)
                           : "transparent"
                }
                Text {
                    anchors.centerIn: parent
                    text: "\u2193"
                    color: downloadArea.containsMouse ? "#89b4fa" : "#7f849c"
                    font.pixelSize: 14
                    font.bold: true
                }
                MouseArea {
                    id: downloadArea
                    anchors.fill: parent
                    hoverEnabled: true
                    ToolTip.text: qsTr("安装")
                    ToolTip.visible: containsMouse
                    ToolTip.delay: 300
                    onClicked: launcher.install(root.agentId_p)
                }
            }

            // Installed: version label + update button
            Item {
                visible: root.installed_p && !root.installing_p
                anchors.fill: parent

                Label {
                    id: versionLabel
                    text: root.version_p.length > 0 ? root.version_p : qsTr("Installed")
                    color: "#7f849c"
                    font.pixelSize: 10
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    elide: Text.ElideRight
                    width: 40
                    horizontalAlignment: Text.AlignHCenter
                }

                // Update icon button (↻)
                Item {
                    id: updateButton
                    anchors.left: versionLabel.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: 16
                    height: 16

                    Rectangle {
                        anchors.fill: parent
                        radius: 8
                        color: updateArea2.containsMouse
                               ? Qt.rgba(137/255, 180/255, 250/255, 0.22)
                               : "transparent"
                    }
                    Text {
                        anchors.centerIn: parent
                        text: "\u21BB"
                        color: updateArea2.containsMouse ? "#89b4fa" : "#7f849c"
                        font.pixelSize: 14
                        font.bold: true
                    }
                    MouseArea {
                        id: updateArea2
                        anchors.fill: parent
                        hoverEnabled: true
                        ToolTip.text: qsTr("更新")
                        ToolTip.visible: containsMouse
                        ToolTip.delay: 300
                        onClicked: launcher.updateTool(root.agentId_p)
                    }
                }
            }
        }

        Column {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 10

            Row {
                spacing: 12
                anchors.horizontalCenter: parent.horizontalCenter

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
                text: root.flashing ? root.flashMessage
                                    : (root.installing_p ? qsTr("Installing…")
                                                          : (root.launching_p ? qsTr("Starting…")
                                                                              : (root.stopping ? qsTr("Stopping…")
                                                                                                : (root.running_p ? qsTr("Running") : qsTr("Stopped")))))
                color: root.flashing ? "#f38ba8"
                                     : ((root.installing_p || root.launching_p || root.stopping || root.running_p) ? root.agentColor : "#7f849c")
                font.pixelSize: 12
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }

            Item { width: 1; height: 4 }

            Row {
                spacing: 10
                width: parent.width

                Button {
                    id: actionButton
                    width: (parent.width - 10) / 2
                    // While the agent is booting up, disable the button (no
                    // double-launch) and show a spinner in place of the label
                    // until the health check confirms it is running.
                    enabled: !root.launching_p
                    text: root.launching_p ? "" : (root.running_p ? qsTr("Open") : qsTr("Start"))

                    background: Rectangle {
                        radius: 8
                        color: parent.down ? Qt.darker(root.agentColor, 1.3)
                                           : (parent.hovered ? Qt.darker(root.agentColor, 1.15) : root.agentColor)
                        opacity: root.launching_p ? 0.6 : 1.0
                    }
                    contentItem: Label {
                        text: parent.text
                        color: "#ffffff"
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    BusyIndicator {
                        anchors.centerIn: parent
                        visible: root.launching_p
                        running: root.launching_p
                        width: 24
                        height: 24
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

        // Subtle "stop" affordance: a faint × in the top-right corner, only
        // while the agent is running. Brightens on hover. Terminates the
        // process tree this launcher started (see AgentLauncher::stop).
        // While stopping, shows a spinner instead of × and is disabled.
        Item {
            id: stopButton
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: 8
            anchors.rightMargin: 8
            width: 22
            height: 22
            visible: root.running_p

            Rectangle {
                anchors.fill: parent
                radius: 11
                color: stopArea.containsMouse ? Qt.rgba(243/255, 139/255, 168/255, 0.22) : "transparent"
            }
            Text {
                anchors.centerIn: parent
                text: "\u00D7"
                color: stopArea.containsMouse ? "#f38ba8" : "#7f849c"
                font.pixelSize: 15
                font.bold: true
                visible: !root.stopping
            }
            BusyIndicator {
                anchors.centerIn: parent
                visible: root.stopping
                running: root.stopping
                width: 16
                height: 16
            }
            MouseArea {
                id: stopArea
                anchors.fill: parent
                hoverEnabled: true
                enabled: !root.stopping
                ToolTip.text: qsTr("关闭")
                ToolTip.visible: containsMouse && !root.stopping
                ToolTip.delay: 300
                onClicked: {
                    root.stopping = true
                    launcher.stop(root.agentId_p)
                }
            }
        }
    }

    function tintRed(hex) { return parseInt(hex.substring(1, 3), 16) / 255 }
    function tintGreen(hex) { return parseInt(hex.substring(3, 5), 16) / 255 }
    function tintBlue(hex) { return parseInt(hex.substring(5, 7), 16) / 255 }
}
