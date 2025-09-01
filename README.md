# Video Compressor

A Qt6-based video compression application with drag-and-drop support, clipboard integration, and FFmpeg automatic installation.

## Features

- **Drag and Drop**: Drop video files directly onto the compressor window
- **Clipboard Integration**: Paste video URLs or file paths with Ctrl+V
- **Multiple Video Formats**: Supports MP4, AVI, MKV, MOV, WMV, FLV, WebM, and more
- **Smart Compression**: Automatically calculates optimal bitrates for target file sizes
- **Hardware Acceleration**: Supports NVIDIA NVENC and Intel QuickSync when available
- **FFmpeg Auto-Install**: One-click FFmpeg installation with administrator privileges
- **Batch Processing**: Compress multiple videos in sequence
- **Smart Temp Cleanup**: Preserves clipboard files while cleaning unused temporary files
- **Real-time Progress**: Live progress tracking with two-pass encoding
- **Thumbnail Generation**: Automatic video thumbnails for easy identification

## Supported Video Formats

### Input Formats

- MP4, AVI, MKV, MOV, WMV, FLV
- WebM, M4V, 3GP, OGV, MPG, MPEG
- TS, M2TS, ASF, RM, RMVB

### Output Format

- MP4 with H.264 video and AAC audio

## Target Sizes

- **10 MB**: Ideal for messaging apps and quick sharing
- **50 MB**: Higher quality for email attachments and cloud storage

## Building

### Prerequisites

- Qt 6.9.2 with dynamic libraries
- CMake 3.16 or higher
- MinGW-w64 compiler
- PowerShell (for build script)

### Build Instructions

1. **Configure Qt Path**:
   Update the Qt path in `CMakeLists.txt` to point to your Qt installation:

   ```cmake
   set(CMAKE_PREFIX_PATH "C:/Qt/6.9.2/mingw_64")
   ```

2. **Run Build Script**:

   **Release Build (default)**:

   ```powershell
   .\Scripts\build.ps1
   ```

   **Debug Build**:

   ```powershell
   .\Scripts\build.ps1 -BuildType Debug
   ```

3. **Manual Build** (alternative):
   ```bash
   cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ```

The executable and all dependencies will be created in the `out/` directory.

## Usage

### Adding Videos

1. **File Dialog**: Click "Add Videos" button
2. **Drag and Drop**: Drag video files from Windows Explorer
3. **Clipboard**: Copy video file paths, then use `Ctrl+V`

### Compression Process

1. **Add Videos**: Use any of the methods above to add videos to the list
2. **Select Target Size**: Choose between 10 MB or 50 MB
3. **Configure Hardware Acceleration**: Enable if available for faster processing
4. **Start Compression**: Click "Compress" to begin batch processing
5. **Monitor Progress**: Watch real-time progress for each video

### Exporting Results

- **Copy to Clipboard**: Copy compressed videos to system clipboard for easy pasting
- **Save to Folder**: Choose a destination folder to save all compressed videos

### FFmpeg Installation

If FFmpeg is not detected:

1. Click the "Install" button next to FFmpeg status
2. Accept the User Account Control (UAC) prompt
3. Wait for automatic installation to complete
4. FFmpeg will be installed system-wide in `C:\Program Files\FFmpeg`

## Hardware Acceleration

The application automatically detects and configures:

- **NVIDIA NVENC (CUDA)**: For NVIDIA GPUs with encoding support
- **Intel QuickSync**: For Intel CPUs with integrated graphics
- **Software Fallback**: Uses CPU-based encoding when hardware acceleration is unavailable

## Smart Features

### Intelligent Bitrate Calculation

- Automatically calculates optimal video bitrates based on target size and video duration
- Applies safety margins to ensure output stays under target size
- Considers audio bitrate (128 kbps AAC) in calculations

### Smart Temporary File Management

- Preserves files currently in clipboard when cleaning temp folders
- Cleans up on startup, compression start, and application exit
- Only removes truly unused temporary files

### Two-Pass Encoding

- First pass: Analyzes video for optimal encoding parameters
- Second pass: Performs actual encoding with optimized settings
- Provides better quality and more accurate file size control

## Keyboard Shortcuts

- `Ctrl+V`: Paste videos from clipboard
- `Ctrl+O`: Open file dialog

## Project Structure

```
├── src/                           # Source code
│   ├── main.cpp                  # Application entry point
│   ├── videocompressor.h/cpp     # Video compression backend
│   └── clipboardmanager.h/cpp    # Clipboard handling
├── qml/                          # QML user interface
│   ├── VideoCompressorWindow.qml # Main window
│   ├── VideoListItem.qml         # Video list item component
│   └── DebugConsole.qml          # Debug console component
├── Scripts/
│   ├── build.ps1                 # Build script
│   └── install-ffmpeg.ps1        # FFmpeg installation script
├── CMakeLists.txt                # CMake configuration
├── app.ico                       # Application icon
└── README.md                     # This file
```

## Dependencies

### Qt Modules

- Qt6::Core
- Qt6::Widgets
- Qt6::Quick
- Qt6::Qml
- Qt6::QuickControls2
- Qt6::Gui

### External Dependencies

- **FFmpeg**: Required for video processing (auto-installable)
- **FFprobe**: Required for video analysis (included with FFmpeg)

## FFmpeg Installation Details

The application includes an automated FFmpeg installer (`install-ffmpeg.ps1`) that:

- Downloads FFmpeg essentials build (~30MB 7z format)
- Extracts using system 7-Zip, portable 7-Zip, or ZIP fallback
- Installs to `C:\Program Files\FFmpeg` (system-wide)
- Automatically adds FFmpeg to system PATH
- Supports forced reinstallation with `-Force` parameter

## Troubleshooting

### FFmpeg Issues

If FFmpeg installation fails:

1. Run PowerShell as Administrator
2. Execute: `.\Scripts\install-ffmpeg.ps1`
3. Restart the application after installation

### Hardware Acceleration Issues

If hardware acceleration isn't detected:

- **NVIDIA**: Ensure latest GPU drivers are installed
- **Intel**: Enable integrated graphics in BIOS/UEFI
- **Software Fallback**: Compression will work but may be slower

### Compression Quality

For better quality:

- Use 50 MB target for higher quality
- Ensure source video resolution isn't too high for target size
- Consider that very short videos may not compress significantly

### Temporary File Issues

The application automatically manages temporary files:

- Startup: Cleans old files while preserving clipboard content
- Compression: Smart cleanup before processing
- Exit: Preserves clipboard files, removes others

### Debug Console

Use the expandable debug console at the bottom to:

- Monitor compression progress
- View detailed FFmpeg output
- Troubleshoot installation issues
- Copy debug information for support

## Performance Tips

1. **Use Hardware Acceleration**: Significantly faster when available
2. **Close Other Applications**: Reduces competition for system resources
3. **SSD Storage**: Faster temporary file operations
4. **Adequate RAM**: 8GB+ recommended for large video files
