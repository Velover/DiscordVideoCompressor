import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    height: 80
    color: index % 2 === 0 ? "#f9f9f9" : "#ffffff"
    border.color: "#e0e0e0"
    border.width: 1

    signal removeRequested(int index)

    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 15

        // Thumbnail
        Rectangle {
            Layout.preferredWidth: 120
            Layout.preferredHeight: 68
            color: "#e0e0e0"
            border.color: "#cccccc"
            border.width: 1

            Image {
                id: thumbnailImage
                anchors.fill: parent
                source: thumbnail || ""
                cache: false
                fillMode: Image.PreserveAspectFit
                visible: thumbnail && thumbnail !== ""
                asynchronous: true

                onStatusChanged: {
                    if (status === Image.Error) {
                        console.log("Thumbnail loading failed for:", fileName);
                    } else if (status === Image.Ready) {
                        console.log("Thumbnail loaded successfully for:", fileName);
                    }
                }
            }

            // Fallback when no thumbnail available
            Text {
                anchors.centerIn: parent
                text: "📹"
                font.pixelSize: 24
                visible: !thumbnailImage.visible || thumbnailImage.status === Image.Error
                color: "#666666"
            }

            // File extension overlay
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                width: 30
                height: 15
                color: "#000000"
                opacity: 0.7
                visible: thumbnailImage.visible && thumbnailImage.status === Image.Ready

                Text {
                    anchors.centerIn: parent
                    text: {
                        var ext = fileName.split('.').pop().toUpperCase();
                        return ext.length <= 4 ? ext : "VID";
                    }
                    color: "white"
                    font.pixelSize: 8
                    font.bold: true
                }
            }

            // Loading indicator for thumbnail generation
            BusyIndicator {
                anchors.centerIn: parent
                running: thumbnailImage.status === Image.Loading
                visible: running
                width: 24
                height: 24
            }
        }

        // File info
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                text: fileName
                font.bold: true
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }

            Text {
                text: originalSize
                color: "#666666"
                font.pixelSize: 12
            }
        }

        // Status
        ColumnLayout {
            Layout.preferredWidth: 150
            spacing: 2

            Text {
                text: statusText
                color: {
                    switch (status) {
                    case 0:
                        return "#666666"; // Ready
                    case 1:
                        return "#2196F3"; // Analyzing
                    case 2:
                        return "#4CAF50"; // AlreadyOptimal
                    case 3:
                        return "#FF9800"; // Compressing
                    case 4:
                        return "#4CAF50"; // Completed
                    case 5:
                        return "#F44336"; // Error
                    default:
                        return "#666666";
                    }
                }
                font.pixelSize: 12
            }

            ProgressBar {
                Layout.fillWidth: true
                visible: status === 3 // Compressing
                value: progress / 100
            }
        }

        Button {
            Layout.preferredWidth: 30
            Layout.preferredHeight: 30
            Layout.alignment: Qt.AlignVCenter
            enabled: !videoCompressor.isCompressing
            font.pixelSize: 14
            font.bold: true

            background: Rectangle {
                color: parent.hovered ? "#ff4444" : "#cccccc"
                radius: 4
                border.color: "#999999"
                border.width: 1
            }

            contentItem: Text {
                anchors.fill: parent                    // Fill the entire button area
                horizontalAlignment: Text.AlignHCenter  // Center text horizontally
                verticalAlignment: Text.AlignVCenter    // Center text vertically
                text: "X"
                color: parent.hovered ? "white" : "#666666"
                font: parent.font
            }

            onClicked: root.removeRequested(index)

            ToolTip.text: "Remove video"
            ToolTip.visible: hovered
        }
    }
}
