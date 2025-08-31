import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#2a2a2a"
    border.color: "#444444"
    border.width: 1

    property var player: null

    // Timer for resuming playback after seeking
    Timer {
        id: resumeTimer
        interval: 150
        repeat: false

        onTriggered: {
            if (player) {
                console.log("Resuming playback after seek");
                player.play();
                // Start the position update timer after resuming
                positionUpdateTimer.start();
            }
        }
    }

    function formatTime(milliseconds) {
        if (!milliseconds || milliseconds < 0)
            return "00:00";

        var seconds = Math.floor(milliseconds / 1000);
        var minutes = Math.floor(seconds / 60);
        var hours = Math.floor(minutes / 60);

        seconds = seconds % 60;
        minutes = minutes % 60;

        if (hours > 0) {
            return String(hours).padStart(2, '0') + ":" + String(minutes).padStart(2, '0') + ":" + String(seconds).padStart(2, '0');
        } else {
            return String(minutes).padStart(2, '0') + ":" + String(seconds).padStart(2, '0');
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
                if (!player)
                    return "▶";
                switch (player.playbackState) {
                case 1:
                    return "⏸";  // PlayingState
                case 2:
                    return "▶";  // PausedState
                case 0:
                    return "▶";  // StoppedState
                default:
                    return "▶";
                }
            }

            onClicked: {
                if (!player)
                    return;
                if (player.playbackState === 1) {
                    // PlayingState
                    player.pause();
                } else {
                    player.play();
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
                    player.stop();
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
            value: player && !pressed && !blockUpdates ? player.position : progressSlider.value

            property bool wasPlaying: false
            property bool blockUpdates: false
            property int lastManualPosition: -1
            property bool seekInProgress: false

            onPressedChanged: {
                if (pressed) {
                    // Store if video was playing before seeking
                    wasPlaying = player && (player.playbackState === 1);
                    blockUpdates = true;
                    seekInProgress = false;
                    // Pause during seeking
                    if (player && wasPlaying) {
                        player.pause();
                    }
                } else {
                    // User released the slider - apply the new position
                    if (player && player.duration > 0 && !seekInProgress) {
                        seekInProgress = true;
                        var newPosition = Math.max(0, Math.min(player.duration, Math.floor(progressSlider.value)));
                        console.log("Manual seek to position:", newPosition, "from:", player.position);

                        // Store the position we're setting for verification
                        lastManualPosition = newPosition;

                        // Set position first
                        player.position = newPosition;

                        // Reset seek flag after a delay
                        seekCompleteTimer.start();

                        // Resume playback if it was playing before
                        if (wasPlaying) {
                            // Use a timer to ensure position is set before resuming
                            resumeTimer.start();
                        } else {
                            // If not resuming playback, allow updates after a short delay
                            positionUpdateTimer.start();
                        }
                    } else {
                        blockUpdates = false;
                    }
                }
            }

            onMoved: {
                if (pressed && player) {
                    // Update time display while dragging
                    currentTimeLabel.text = formatTime(progressSlider.value);
                }
            }

            // Update slider position only when appropriate
            Connections {
                target: player
                function onPositionChanged() {
                    if (!progressSlider.pressed && !progressSlider.blockUpdates) {
                        // Only update if the position change is significant (not from our manual change)
                        var currentPos = player.position;
                        var timeDiff = Math.abs(currentPos - progressSlider.lastManualPosition);

                        // If we recently set a manual position, make sure enough time has passed
                        // or the position has changed significantly from what we set
                        if (progressSlider.lastManualPosition < 0 || timeDiff > 1000) {
                            progressSlider.value = currentPos;
                            currentTimeLabel.text = formatTime(currentPos);
                        }
                    }
                }
            }

            // Timer to complete seek operation
            Timer {
                id: seekCompleteTimer
                interval: 300
                repeat: false
                onTriggered: {
                    progressSlider.seekInProgress = false;
                }
            }

            // Timer to re-enable position updates after seeking
            Timer {
                id: positionUpdateTimer
                interval: 400
                repeat: false
                onTriggered: {
                    progressSlider.blockUpdates = false;
                    progressSlider.lastManualPosition = -1;
                    console.log("Re-enabled position updates");
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
                    if (!player)
                        return "🔊";
                    if (player.muted === true)
                        return "🔇";
                    var vol = player.volume || 0;
                    if (vol === 0)
                        return "🔇";
                    if (vol < 0.3)
                        return "🔈";
                    if (vol < 0.7)
                        return "🔉";
                    return "🔊";
                }

                onClicked: {
                    if (player) {
                        player.muted = !player.muted;
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
                        player.volume = value;
                        player.muted = false;
                    }
                }
            }
        }
    }
}
