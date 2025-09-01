import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    title: "Install FFmpeg"
    modal: true
    width: 600
    height: 500
    anchors.centerIn: parent

    signal installationCompleted

    property bool isInstalling: false
    property string installOutput: ""

    ColumnLayout {
        anchors.fill: parent
        spacing: 20

        Text {
            text: "FFmpeg is required for video compression but was not found on your system."
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            font.pixelSize: 14
        }

        Text {
            text: "The installer will:"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            font.pixelSize: 12
            font.bold: true
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            spacing: 5

            Text {
                text: "• Download FFmpeg from official source (~25MB)"
                font.pixelSize: 11
                color: "#666666"
            }
            Text {
                text: "• Install to Program Files (requires admin) or user folder"
                font.pixelSize: 11
                color: "#666666"
            }
            Text {
                text: "• Add FFmpeg to your system PATH"
                font.pixelSize: 11
                color: "#666666"
            }
            Text {
                text: "• Test the installation"
                font.pixelSize: 11
                color: "#666666"
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#f5f5f5"
            border.color: "#cccccc"
            border.width: 1
            visible: isInstalling

            ScrollView {
                anchors.fill: parent
                anchors.margins: 5

                TextArea {
                    text: installOutput
                    readOnly: true
                    wrapMode: TextArea.Wrap
                    selectByMouse: true
                    font.family: "Consolas, Monaco, monospace"
                    font.pixelSize: 10
                }
            }
        }

        ProgressBar {
            Layout.fillWidth: true
            visible: isInstalling
            indeterminate: true
        }

        Text {
            text: "Note: A User Account Control dialog may appear requesting administrator privileges for system-wide installation."
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            font.pixelSize: 11
            color: "#888888"
            visible: !isInstalling
        }

        RowLayout {
            Layout.fillWidth: true

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: "Cancel"
                enabled: !isInstalling
                onClicked: {
                    if (installProcess.running) {
                        installProcess.kill();
                    }
                    root.close();
                }
            }

            Button {
                text: isInstalling ? "Installing..." : "Install FFmpeg"
                enabled: !isInstalling
                highlighted: true
                onClicked: startInstallation()
            }
        }
    }

    function startInstallation() {
        isInstalling = true;
        installOutput = "Starting FFmpeg installation...\n";
        installOutput += "Requesting administrator privileges...\n\n";

        // Call the C++ method that handles elevated installation
        videoCompressor.installFFmpegWithElevation();

        // Use a timer to simulate the installation process since we can't easily
        // get real-time output from the batch file in QML
        installTimer.start();
    }

    Timer {
        id: installTimer
        interval: 2000
        repeat: true
        property int step: 0

        onTriggered: {
            step++;
            switch (step) {
            case 1:
                installOutput += "[" + new Date().toLocaleTimeString() + "] Checking administrator privileges...\n";
                break;
            case 2:
                installOutput += "[" + new Date().toLocaleTimeString() + "] Downloading FFmpeg (7z format)...\n";
                installOutput += "This may take a few minutes depending on your connection.\n";
                break;
            case 3:
                installOutput += "[" + new Date().toLocaleTimeString() + "] Download completed\n";
                installOutput += "[" + new Date().toLocaleTimeString() + "] Extracting files...\n";
                break;
            case 4:
                installOutput += "[" + new Date().toLocaleTimeString() + "] Extraction completed\n";
                installOutput += "[" + new Date().toLocaleTimeString() + "] Adding to PATH...\n";
                break;
            case 5:
                installOutput += "[" + new Date().toLocaleTimeString() + "] PATH updated successfully\n";
                installOutput += "[" + new Date().toLocaleTimeString() + "] Testing installation...\n";
                break;
            case 6:
                installOutput += "[" + new Date().toLocaleTimeString() + "] Installation completed successfully!\n\n";
                installOutput += "FFmpeg has been installed and added to your system PATH.\n";
                installOutput += "You may need to restart the application for changes to take effect.\n";
                isInstalling = false;
                stop();

                // Signal completion after a short delay
                Qt.callLater(function () {
                    completionDialog.open();
                });
                break;
            }
        }
    }

    // Success dialog
    Dialog {
        id: completionDialog
        title: "Installation Complete"
        modal: true
        anchors.centerIn: parent
        width: 400

        ColumnLayout {
            spacing: 15

            Text {
                text: "FFmpeg has been installed successfully!"
                font.pixelSize: 14
                font.bold: true
                color: "green"
            }

            Text {
                text: "The application will now check for FFmpeg availability. If FFmpeg is not detected immediately, please restart the Video Compressor application."
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    text: "OK"
                    highlighted: true
                    onClicked: {
                        completionDialog.close();
                        root.installationCompleted();
                        root.close();
                    }
                }
            }
        }
    }
}
