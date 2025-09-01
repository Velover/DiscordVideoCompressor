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

        // Thumbnail placeholder
        Rectangle {
            Layout.preferredWidth: 60
            Layout.preferredHeight: 40
            color: "#e0e0e0"
            border.color: "#cccccc"
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "📹"
                font.pixelSize: 20
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

        // Remove button
        Button {
            text: "X"
            Layout.preferredWidth: 30
            Layout.preferredHeight: 30
            enabled: !videoCompressor.isCompressing
            onClicked: root.removeRequested(index)
        }
    }
}
