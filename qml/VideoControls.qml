import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#2a2a2a"
    border.color: "#444444"
    border.width: 1
    
    property var player: null
    
    function formatTime(milliseconds) {
        if (!milliseconds || milliseconds < 0) return "00:00"
        
        var seconds = Math.floor(milliseconds / 1000)
        var minutes = Math.floor(seconds / 60)
        var hours = Math.floor(minutes / 60)
        
        seconds = seconds % 60
        minutes = minutes % 60
        
        if (hours > 0) {
            return String(hours).padStart(2, '0') + ":" + 
                   String(minutes).padStart(2, '0') + ":" + 
                   String(seconds).padStart(2, '0')
        } else {
            return String(minutes).padStart(2, '0') + ":" + 
                   String(seconds).padStart(2, '0')
        }
    }
    
    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 15
        
        // Play/Pause button
        Button {
            id: playButton
            Layout.preferredWidth: 50
            Layout.preferredHeight: 40
            
            text: {
                if (!player) return "▶"
                switch (player.playbackState) {
                    case 1: return "⏸"  // PlayingState
                    case 2: return "▶"  // PausedState
                    case 0: return "▶"  // StoppedState
                    default: return "▶"
                }
            }
            
            onClicked: {
                if (!player) return
                
                if (player.playbackState === 1) {  // PlayingState
                    player.pause()
                } else {
                    player.play()
                }
            }
        }
        
        // Stop button
        Button {
            Layout.preferredWidth: 50
            Layout.preferredHeight: 40
            text: "⏹"
            
            onClicked: {
                if (player) {
                    player.stop()
                }
            }
        }
        
        // Current time
        Label {
            id: currentTimeLabel
            color: "white"
            text: player ? formatTime(player.position) : "00:00"
            Layout.preferredWidth: 60
        }
        
        // Progress slider
        Slider {
            id: progressSlider
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            
            from: 0
            to: player ? player.duration : 1
            value: player ? player.position : 0
            
            property bool seeking: false
            
            onPressedChanged: {
                seeking = pressed
                if (!pressed && player) {
                    player.setPosition(value)
                }
            }
            
            onMoved: {
                if (seeking && player) {
                    // Update position while dragging for preview
                    currentTimeLabel.text = formatTime(value)
                }
            }
        }
        
        // Duration time
        Label {
            id: durationLabel
            color: "white"
            text: player ? formatTime(player.duration) : "00:00"
            Layout.preferredWidth: 60
        }
        
        // Volume control
        RowLayout {
            spacing: 5
            
            Button {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                text: {
                    if (!player) return "🔊"
                    if (player.muted === true) return "🔇"
                    var vol = player.volume || 0
                    if (vol === 0) return "🔇"
                    if (vol < 0.3) return "🔈"
                    if (vol < 0.7) return "🔉"
                    return "🔊"
                }
                
                onClicked: {
                    if (player) {
                        player.muted = !player.muted
                    }
                }
            }
            
            Slider {
                id: volumeSlider
                Layout.preferredWidth: 100
                Layout.preferredHeight: 40
                
                from: 0
                to: 1
                value: (player && player.volume !== undefined) ? player.volume : 0.5
                
                onMoved: {
                    if (player) {
                        player.volume = value
                        player.muted = false
                    }
                }
            }
        }
    }
}
