import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtMultimedia

ApplicationWindow {
    id: window
    visible: true
    width: 1280
    height: 720
    title: "Video Player - " + (videoPlayer.currentSource ? videoPlayer.currentSource : "No video loaded")
    
    property bool isFullScreen: false
    
    // File dialog for opening videos
    FileDialog {
        id: fileDialog
        title: "Select Video File"
        nameFilters: ["Video files (*.mp4 *.avi *.mkv *.mov *.wmv *.flv *.webm *.m4v *.3gp *.ogv *.mpg *.mpeg)", "All files (*)"]
        onAccepted: {
            videoPlayer.loadVideo(selectedFile)
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
        
        onDropped: function(drop) {
            if (drop.hasUrls) {
                for (var i = 0; i < drop.urls.length; i++) {
                    var url = drop.urls[i]
                    console.log("Dropped URL:", url)
                    videoPlayer.loadVideo(url)
                    break // Load only the first video
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
                text: "Drop video file here"
                font.pixelSize: 24
                color: "blue"
                visible: dropArea.containsDrag
            }
            
            Behavior on opacity {
                NumberAnimation { duration: 200 }
            }
        }
    }
    
    // Main content area
    Rectangle {
        anchors.fill: parent
        color: "black"
        
        VideoOutput {
            id: videoOutput
            anchors.fill: parent
            fillMode: VideoOutput.PreserveAspectFit
            
            Component.onCompleted: {
                console.log("VideoOutput created")
                videoPlayer.setVideoOutput(videoOutput)
            }
            
            Connections {
                target: videoPlayer
                function onPlaybackStateChanged() {
                    console.log("Playback state:", videoPlayer.playbackState)
                    // Force update when paused
                    if (videoPlayer.playbackState === MediaPlayer.PausedState) {
                        videoOutput.update()
                    }
                }
            }
            
            // Click to play/pause
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (videoPlayer.playbackState === MediaPlayer.PlayingState) {
                        videoPlayer.pause()
                    } else {
                        videoPlayer.play()
                    }
                }
                onDoubleClicked: {
                    if (isFullScreen) {
                        window.showNormal()
                    } else {
                        window.showFullScreen()
                    }
                    isFullScreen = !isFullScreen
                }
            }
        }
        
        // Loading indicator
        BusyIndicator {
            anchors.centerIn: parent
            visible: videoPlayer.playbackState === MediaPlayer.PlayingState && videoPlayer.position === 0
        }
        
        // Placeholder text when no video is loaded
        Text {
            anchors.centerIn: parent
            text: "Drop video file here or press Ctrl+V to paste from clipboard"
            color: "white"
            font.pixelSize: 18
            visible: !videoPlayer.currentSource
        }
        
        // Video controls
        VideoControls {
            id: videoControls
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 80
            
            player: videoPlayer
            visible: !isFullScreen || controlsTimer.running
        }
    }
    
    // Auto-hide controls in fullscreen
    Timer {
        id: controlsTimer
        interval: 3000
        running: false
    }
    
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        hoverEnabled: isFullScreen
        onPositionChanged: {
            if (isFullScreen) {
                controlsTimer.restart()
            }
        }
    }
    
    // Keyboard shortcuts
    Shortcut {
        sequence: "Space"
        onActivated: {
            if (videoPlayer.playbackState === MediaPlayer.PlayingState) {
                videoPlayer.pause()
            } else {
                videoPlayer.play()
            }
        }
    }
    
    Shortcut {
        sequence: "Ctrl+V"
        onActivated: {
            if (clipboardManager.hasVideoUrl()) {
                var url = clipboardManager.getVideoUrl()
                if (url.toString() !== "") {
                    videoPlayer.loadVideo(url)
                }
            } else {
                errorDialog.text = "No valid video URL found in clipboard"
                errorDialog.open()
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+O"
        onActivated: fileDialog.open()
    }

    Shortcut {
        sequence: "F11"
        onActivated: {
            if (isFullScreen) {
                window.showNormal()
            } else {
                window.showFullScreen()
            }
            isFullScreen = !isFullScreen
        }
    }

    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (isFullScreen) {
                window.showNormal()
                isFullScreen = false
            }
        }
    }
}