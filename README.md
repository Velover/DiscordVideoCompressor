# Video Player

A Qt6-based video player application with drag-and-drop support and clipboard integration.

## Features

- **Drag and Drop**: Drop video files directly onto the player window
- **Clipboard Integration**: Paste video URLs or file paths with Ctrl+V
- **Multiple Video Formats**: Supports MP4, AVI, MKV, MOV, WMV, FLV, WebM, and more
- **Full Screen Mode**: Double-click video or press F11 for full screen
- **Video Controls**: Play, pause, stop, seek, volume control
- **Static Build**: Self-contained executable with no external Qt dependencies

## Supported Video Formats

- MP4, AVI, MKV, MOV, WMV, FLV
- WebM, M4V, 3GP, OGV, MPG, MPEG
- TS, M2TS, ASF, RM, RMVB

## Building

### Prerequisites

- Qt 6.9.2 with static libraries
- CMake 3.16 or higher
- MinGW-w64 compiler
- PowerShell (for build script)

### Build Instructions

1. **Configure Qt Static Build Path**:
   Update the Qt path in `CMakeLists.txt` to point to your Qt static installation:

   ```cmake
   set(CMAKE_PREFIX_PATH "C:/Qt/6.9.2/Static/mingw_64")
   ```

2. **Run Build Script**:

   ```powershell
   .\Scripts\build.ps1
   ```

3. **Manual Build** (alternative):
   ```bash
   cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ```

The executable will be created in the `out/` directory.

## Usage

### Opening Videos

1. **File Menu**: Use `File > Open...` or `Ctrl+O`
2. **Drag and Drop**: Drag video files from Windows Explorer
3. **Clipboard**: Copy a video file path or URL, then use `File > Paste from Clipboard` or `Ctrl+V`

### Keyboard Shortcuts

- `Space`: Play/Pause
- `Ctrl+O`: Open file dialog
- `Ctrl+V`: Paste from clipboard
- `F11`: Toggle full screen
- `Escape`: Exit full screen
- `Ctrl+Q`: Quit application

### Mouse Controls

- **Single Click**: Play/Pause
- **Double Click**: Toggle full screen
- **Mouse Movement** (in full screen): Show controls temporarily

## Project Structure

```
├── src/                    # Source code
│   ├── main.cpp           # Application entry point
│   ├── videoplayer.h/cpp  # Video player backend
│   └── clipboardmanager.h/cpp # Clipboard handling
├── qml/                   # QML user interface
│   ├── VideoPlayerWindow.qml  # Main window
│   └── VideoControls.qml      # Video controls component
├── Scripts/
│   └── build.ps1          # Build script
├── CMakeLists.txt         # CMake configuration
└── README.md              # This file
```

## Dependencies

The application uses the following Qt modules:

- Qt6::Core
- Qt6::Widgets
- Qt6::Multimedia
- Qt6::MultimediaWidgets
- Qt6::Quick
- Qt6::Qml
- Qt6::QuickControls2

## Troubleshooting

### Static Build Issues

If you encounter issues with static builds:

1. Ensure you have Qt built with static libraries
2. Verify the `CMAKE_PREFIX_PATH` points to the correct Qt installation
3. Check that all required Qt plugins are available in the static build

### Video Playback Issues

- Ensure Windows has the necessary codec support
- Try different video formats if one doesn't work
- Check that the video file is not corrupted

### Clipboard Detection

The clipboard manager detects:

- Local video file paths
- Video URLs from common streaming services
- Direct video file URLs

If clipboard paste doesn't work, ensure the clipboard contains a valid video file path or URL.
