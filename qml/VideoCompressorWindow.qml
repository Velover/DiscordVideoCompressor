import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: window
    visible: true
    width: 1000
    height: 700
    title: "Video Compressor"

    property alias debugConsole: debugConsole

    // File dialog for opening videos
    FileDialog {
        id: fileDialog
        title: "Select Video Files"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Video files (*.mp4 *.avi *.mkv *.mov *.wmv *.flv *.webm *.m4v *.3gp *.ogv *.mpg *.mpeg)", "All files (*)"]
        onAccepted: {
            for (var i = 0; i < selectedFiles.length; i++) {
                videoCompressor.addVideo(selectedFiles[i]);
            }
        }
    }

    // Folder dialog for saving
    FolderDialog {
        id: folderDialog
        title: "Select Output Folder"
        onAccepted: {
            videoCompressor.saveToFolder(selectedFolder);
        }
    }

    // Error dialog
    Dialog {
        id: errorDialog
        title: "Error"
        property alias text: errorText.text
        modal: true
        anchors.centerIn: parent

        Text {
            id: errorText
            wrapMode: Text.WordWrap
            width: parent.width
        }

        standardButtons: Dialog.Ok
    }

    // Drag and drop area
    DropArea {
        id: dropArea
        anchors.fill: parent

        onDropped: function (drop) {
            if (drop.hasUrls) {
                for (var i = 0; i < drop.urls.length; i++) {
                    videoCompressor.addVideo(drop.urls[i]);
                }
            }
        }

        Rectangle {
            id: dropIndicator
            anchors.fill: parent
            color: "lightblue"
            opacity: dropArea.containsDrag ? 0.3 : 0
            border.color: "blue"
            border.width: 2

            Text {
                anchors.centerIn: parent
                text: "Drop video files here"
                font.pixelSize: 24
                color: "blue"
                visible: dropArea.containsDrag
            }

            Behavior on opacity {
                NumberAnimation {
                    duration: 200
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        // Header with controls
        RowLayout {
            Layout.fillWidth: true
            spacing: 15

            Text {
                text: "Video Compressor"
                font.pixelSize: 24
                font.bold: true
            }

            Item {
                Layout.fillWidth: true
            }

            // FFmpeg status
            RowLayout {
                spacing: 5

                Rectangle {
                    width: 12
                    height: 12
                    radius: 6
                    color: videoCompressor.ffmpegAvailable ? "green" : "red"
                }

                Text {
                    text: videoCompressor.ffmpegAvailable ? "FFmpeg Ready" : "FFmpeg Not Found"
                    color: videoCompressor.ffmpegAvailable ? "green" : "red"
                }

                Button {
                    text: "Install"
                    visible: !videoCompressor.ffmpegAvailable
                    onClicked: {
                        debugConsole.addMessage("Starting FFmpeg installation with administrator privileges...", "info");
                        videoCompressor.installFFmpegWithElevation();
                    }
                }
            }
        }

        // Controls row
        RowLayout {
            Layout.fillWidth: true
            spacing: 15

            Button {
                text: "Add Videos"
                onClicked: fileDialog.open()
            }

            Button {
                text: "Clear All"
                enabled: !videoCompressor.isCompressing
                onClicked: videoCompressor.clearVideos()
            }

            Item {
                width: 20
            }

            Text {
                text: "Target Size:"
            }

            ComboBox {
                id: sizeComboBox
                model: ["10 MB", "50 MB"]
                currentIndex: videoCompressor.targetSizeMB === 10 ? 0 : 1
                onCurrentIndexChanged: {
                    videoCompressor.targetSizeMB = currentIndex === 0 ? 10 : 50;
                }
            }

            Item {
                width: 20
            }

            // Hardware Acceleration Checkbox
            RowLayout {
                spacing: 5

                CheckBox {
                    id: hwAccelCheckbox
                    checked: videoCompressor.hardwareAccelerationEnabled
                    enabled: videoCompressor.hardwareAccelerationAvailable && !videoCompressor.isCompressing

                    onCheckedChanged: {
                        videoCompressor.hardwareAccelerationEnabled = checked;
                    }

                    ToolTip.text: videoCompressor.hardwareAccelerationAvailable ? "Hardware acceleration using " + videoCompressor.hardwareAccelerationType : "Hardware acceleration not available"
                    ToolTip.visible: hovered
                }

                Text {
                    text: "Hardware Acceleration"
                    color: videoCompressor.hardwareAccelerationAvailable ? "#000000" : "#888888"
                }

                Rectangle {
                    width: 12
                    height: 12
                    radius: 6
                    color: {
                        if (!videoCompressor.hardwareAccelerationAvailable)
                            return "#888888";
                        return videoCompressor.hardwareAccelerationEnabled ? "#4CAF50" : "#FF9800";
                    }

                    ToolTip.text: {
                        if (!videoCompressor.hardwareAccelerationAvailable) {
                            return "Hardware acceleration not available";
                        }
                        return videoCompressor.hardwareAccelerationEnabled ? "Hardware acceleration enabled: " + videoCompressor.hardwareAccelerationType : "Hardware acceleration disabled";
                    }
                    ToolTip.visible: mouseArea.containsMouse

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                    }
                }
            }

            Button {
                text: "Compress"
                enabled: !videoCompressor.isCompressing && videoCompressor.totalCount > 0 && videoCompressor.ffmpegAvailable
                onClicked: videoCompressor.startCompression()
            }

            Item {
                Layout.fillWidth: true
            }

            // Progress info
            Text {
                text: videoCompressor.completedCount + " / " + videoCompressor.totalCount + " completed"
                visible: videoCompressor.totalCount > 0
            }
        }

        // Video list
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            border.color: "#cccccc"
            border.width: 1

            ScrollView {
                anchors.fill: parent
                anchors.margins: 1

                ListView {
                    id: videoList
                    model: videoCompressor
                    spacing: 5

                    delegate: VideoListItem {
                        width: videoList.width
                        onRemoveRequested: function (index) {
                            videoCompressor.removeVideo(index);
                        }
                    }
                }
            }

            // Placeholder when empty
            Text {
                anchors.centerIn: parent
                text: "Drop video files here or click 'Add Videos' to get started"
                color: "#666666"
                font.pixelSize: 16
                visible: videoCompressor.totalCount === 0
            }
        }

        // Debug Console
        DebugConsole {
            id: debugConsole
            Layout.fillWidth: true
            Layout.preferredHeight: collapsed ? 30 : 200
            Layout.minimumHeight: 30
        }

        // Export buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 15

            Button {
                text: "Copy to Clipboard"
                enabled: videoCompressor.completedCount > 0
                onClicked: {
                    videoCompressor.copyToClipboard();
                    debugConsole.addMessage("Preparing " + videoCompressor.completedCount + " videos for clipboard...", "info");
                }
            }

            Button {
                text: "Save As..."
                enabled: videoCompressor.completedCount > 0
                onClicked: {
                    debugConsole.addMessage("Opening folder selection dialog...", "info");
                    folderDialog.open();
                }
            }

            Item {
                Layout.fillWidth: true
            }

            // Add info about output location
            Text {
                text: "Output: ./output/"
                color: "#666666"
                font.pixelSize: 10
                visible: videoCompressor.completedCount > 0
                ToolTip.text: "Compressed videos are stored in the output directory for easy access"
                ToolTip.visible: outputMouseArea.containsMouse

                MouseArea {
                    id: outputMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                }
            }
        }
    }

    // Connections
    Connections {
        target: videoCompressor
        function onError(message) {
            errorDialog.text = message;
            errorDialog.open();
            debugConsole.addMessage("ERROR: " + message, "error");
        }
        function onCompressionFinished() {
            debugConsole.addMessage("Compression batch completed", "success");
        }
        function onTargetSizeMBChanged() {
            debugConsole.addMessage("Target size changed to: " + videoCompressor.targetSizeMB + " MB");
        }
        function onIsCompressingChanged() {
            if (videoCompressor.isCompressing) {
                debugConsole.addMessage("Starting compression batch...", "info");
            } else {
                debugConsole.addMessage("Compression stopped", "info");
            }
        }
        function onDebugMessage(message, type) {
            debugConsole.addMessage(message, type);
        }
        function onHardwareAccelerationAvailableChanged() {
            if (videoCompressor.hardwareAccelerationAvailable) {
                debugConsole.addMessage("Hardware acceleration detected: " + videoCompressor.hardwareAccelerationType, "success");
            } else {
                debugConsole.addMessage("No hardware acceleration available", "warning");
            }
        }
        function onHardwareAccelerationEnabledChanged() {
            if (videoCompressor.hardwareAccelerationEnabled) {
                debugConsole.addMessage("Hardware acceleration enabled", "success");
            } else {
                debugConsole.addMessage("Hardware acceleration disabled", "info");
            }
        }
    }

    // Keyboard shortcuts
    Shortcut {
        sequence: "Ctrl+V"
        onActivated: {
            if (clipboardManager.hasVideoUrl()) {
                var urls = clipboardManager.getAllVideoUrls();
                if (urls.length > 0) {
                    for (var i = 0; i < urls.length; i++) {
                        videoCompressor.addVideo(urls[i]);
                    }
                    if (urls.length === 1) {
                        debugConsole.addMessage("Added video from clipboard (Ctrl+V): " + urls[0].toString(), "success");
                    } else {
                        debugConsole.addMessage("Added " + urls.length + " videos from clipboard (Ctrl+V)", "success");
                    }
                }
            } else {
                debugConsole.addMessage("No valid video URL found in clipboard", "warning");
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+O"
        onActivated: fileDialog.open()
    }
}
