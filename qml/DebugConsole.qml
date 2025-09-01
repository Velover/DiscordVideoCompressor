import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#1e1e1e"
    border.color: "#444444"
    border.width: 1

    property bool collapsed: true
    property alias messageCount: messageModel.count

    function addMessage(text, type = "info") {
        var timestamp = new Date().toLocaleTimeString();
        var color = getMessageColor(type);
        messageModel.append({
            "timestamp": timestamp,
            "message": text,
            "type": type,
            "color": color
        });

        // Auto-scroll to bottom
        if (!collapsed) {
            Qt.callLater(function () {
                if (messageList.count > 0) {
                    messageList.positionViewAtEnd();
                }
            });
        }

        // Limit message count to prevent memory issues
        if (messageModel.count > 1000) {
            messageModel.remove(0, 100);
        }
    }

    function getMessageColor(type) {
        switch (type) {
        case "error":
            return "#ff6b6b";
        case "warning":
            return "#ffa726";
        case "success":
            return "#66bb6a";
        case "info":
            return "#42a5f5";
        default:
            return "#ffffff";
        }
    }

    function clearMessages() {
        messageModel.clear();
    }

    Behavior on Layout.preferredHeight {
        NumberAnimation {
            duration: 200
            easing.type: Easing.OutCubic
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            color: "#2d2d2d"
            border.color: "#444444"
            border.width: 0

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 5
                spacing: 10

                Text {
                    text: "Debug Console"
                    color: "#ffffff"
                    font.pixelSize: 12
                    font.bold: true
                }

                Text {
                    text: "(" + messageModel.count + " messages)"
                    color: "#888888"
                    font.pixelSize: 10
                }

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    text: "🗑"
                    font.pixelSize: 10
                    enabled: messageModel.count > 0
                    ToolTip.text: "Clear messages"
                    ToolTip.visible: hovered
                    onClicked: clearMessages()
                }

                Button {
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    text: collapsed ? "▲" : "▼"
                    font.pixelSize: 10
                    ToolTip.text: collapsed ? "Expand console" : "Collapse console"
                    ToolTip.visible: hovered
                    onClicked: collapsed = !collapsed
                }
            }
        }

        // Message area
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !collapsed
            clip: true

            ListView {
                id: messageList
                model: messageModel
                spacing: 1

                delegate: Rectangle {
                    width: messageList.width
                    height: messageText.contentHeight + 6
                    color: index % 2 === 0 ? "#252525" : "#2a2a2a"

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 3
                        spacing: 10

                        Text {
                            text: model.timestamp
                            color: "#888888"
                            font.pixelSize: 9
                            font.family: "Consolas, Monaco, monospace"
                            Layout.preferredWidth: 60
                        }

                        Rectangle {
                            Layout.preferredWidth: 8
                            Layout.preferredHeight: 8
                            radius: 4
                            color: model.color
                        }

                        TextEdit {
                            id: messageText
                            text: model.message
                            color: model.color
                            font.pixelSize: 10
                            font.family: "Consolas, Monaco, monospace"
                            Layout.fillWidth: true
                            wrapMode: TextEdit.Wrap
                            readOnly: true
                            selectByMouse: true
                            selectionColor: "#555555"
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton
                        onClicked: {
                            contextMenu.popup();
                        }

                        Menu {
                            id: contextMenu

                            MenuItem {
                                text: "Copy Message"
                                onTriggered: {
                                    // Copy message to clipboard
                                    messageText.selectAll();
                                    messageText.copy();
                                    messageText.deselect();
                                }
                            }

                            MenuItem {
                                text: "Copy All Messages"
                                onTriggered: {
                                    var allMessages = "";
                                    for (var i = 0; i < messageModel.count; i++) {
                                        var item = messageModel.get(i);
                                        allMessages += "[" + item.timestamp + "] " + item.message + "\n";
                                    }
                                    // Create a temporary TextEdit to copy all messages
                                    var tempText = Qt.createQmlObject('import QtQuick; TextEdit { text: "' + allMessages.replace(/"/g, '\\"') + '" }', root, "tempTextEdit");
                                    tempText.selectAll();
                                    tempText.copy();
                                    tempText.destroy();
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    ListModel {
        id: messageModel
    }

    Component.onCompleted: {
        addMessage("Debug console initialized", "info");
        addMessage("Video Compressor ready", "success");
    }
}
