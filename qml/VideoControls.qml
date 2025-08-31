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
            value: player && !seeking ? player.position : progressSlider.value
            
            property bool seeking: false
            property bool wasPlaying: false
            property bool isDragging: false
            
            // Disable live updates to prevent click-through
            live: false
            
            onPressedChanged: {
                if (pressed) {
                    // Store if video was playing before seeking (1 = PlayingState)
                    wasPlaying = player && (player.playbackState === 1)
                    seeking = true
                    isDragging = false
                    // Pause during seeking to prevent interference
                    if (player && wasPlaying) {
                        player.pause()
                    }
                } else {
                    // User released the slider - only seek if it was actually dragged
                    if (player && player.duration > 0 && (isDragging || Math.abs(progressSlider.value - player.position) > 1000)) {
                        // Set the new position with bounds checking
                        var newPosition = Math.max(0, Math.min(player.duration, Math.floor(progressSlider.value)))
                        console.log("Seeking to position:", newPosition, "from:", player.position)
                        player.position = newPosition
                        // Resume playing only if it was playing before
                        if (wasPlaying) {
                            player.play()
                        }
                    } else if (!isDragging) {
                        // If it was just a click without drag, restore original value
                        progressSlider.value = player ? player.position : 0
                    }
                    seeking = false
                    isDragging = false
                }
            }
            
            onMoved: {
                // Mark that the user is actually dragging
                isDragging = true
                if (seeking && player) {
                    // Update time display while dragging for preview
                    currentTimeLabel.text = formatTime(progressSlider.value)
                } else if (!seeking && player) {
                    // Restore normal time display when not seeking
                    currentTimeLabel.text = formatTime(player.position)
                }
            }
            
            // Prevent automatic updates while user is seeking
            Connections {
                target: player
                function onPositionChanged() {
                    if (!progressSlider.seeking) {
                        progressSlider.value = player.position
                    }
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
