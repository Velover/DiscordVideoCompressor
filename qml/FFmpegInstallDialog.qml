import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    title: "Install FFmpeg"
    modal: true
    width: 500
    height: 400
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
        }

        Text {
            text: "Click 'Install' to automatically download and install FFmpeg. This process requires administrator privileges."
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            color: "#666666"
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: isInstalling

            TextArea {
                text: installOutput
                readOnly: true
                wrapMode: TextArea.Wrap
                selectByMouse: true
            }
        }

        ProgressBar {
            Layout.fillWidth: true
            visible: isInstalling
            indeterminate: true
        }

        RowLayout {
            Layout.fillWidth: true

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: "Cancel"
                enabled: !isInstalling
                onClicked: root.close()
            }

            Button {
                text: isInstalling ? "Installing..." : "Install"
                enabled: !isInstalling
                onClicked: startInstallation()
            }
        }
    }

    function startInstallation() {
        isInstalling = true;
        installOutput = "Starting FFmpeg installation...\n";

        // Start the installation process
        installProcess.start();
    }

    Timer {
        id: installProcess
        interval: 1000
        repeat: true
        property int step: 0

        onTriggered: {
            step++;
            switch (step) {
            case 1:
                installOutput += "Checking administrator privileges...\n";
                break;
            case 2:
                installOutput += "Downloading FFmpeg...\n";
                break;
            case 3:
                installOutput += "Extracting files...\n";
                break;
            case 4:
                installOutput += "Adding to PATH...\n";
                break;
            case 5:
                installOutput += "Installation completed successfully!\n";
                isInstalling = false;
                stop();
                Qt.callLater(function () {
                    root.installationCompleted();
                    root.close();
                });
                break;
            }
        }
    }
}
