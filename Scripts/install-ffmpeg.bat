@echo off
setlocal enabledelayedexpansion

echo ========================================
echo FFmpeg Installation Script (7z Version)
echo ========================================

REM Check if FFmpeg is already installed
where ffmpeg >nul 2>&1
if %ERRORLEVEL% == 0 (
    echo FFmpeg is already installed and available in PATH.
    ffmpeg -version | findstr "ffmpeg version"
    pause
    exit /b 0
)

echo FFmpeg not found. Starting installation...

REM Initialize ADMIN_MODE variable
set ADMIN_MODE=0
REM Check if running as administrator
net session >nul 2>&1
if %ERRORLEVEL% == 0 (
    echo Running with administrator privileges.
    set ADMIN_MODE=1
) else (
    echo Running without administrator privileges.
    set ADMIN_MODE=0
)


REM Check if running as administrator
net session >nul 2>&1
if %ADMIN_MODE% == 1 (
    set "FFMPEG_DIR=C:\Program Files\FFmpeg"
    echo Installing to: %FFMPEG_DIR% (System-wide)
) else (
    set "FFMPEG_DIR=%LOCALAPPDATA%\FFmpeg"
    echo Installing to: %FFMPEG_DIR% (User-only)
)

REM Create installation directory
if not exist "%FFMPEG_DIR%" (
    mkdir "%FFMPEG_DIR%"
)

echo Downloading FFmpeg (7z format - smaller download)...
REM Download 7z version (much smaller)
powershell -Command "& {try { Invoke-WebRequest -Uri 'https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.7z' -OutFile '%FFMPEG_DIR%\ffmpeg.7z' -UseBasicParsing } catch { Write-Host 'Download failed: ' $_.Exception.Message; exit 1 }}"

if %ERRORLEVEL% neq 0 (
    echo ERROR: Failed to download FFmpeg
    pause
    exit /b 1
)

echo Extracting FFmpeg (7z format)...

REM Method 1: Try using 7-Zip if installed
where 7z >nul 2>&1
if %ERRORLEVEL% == 0 (
    echo Using installed 7-Zip...
    7z x "%FFMPEG_DIR%\ffmpeg.7z" -o"%FFMPEG_DIR%" -y
    set EXTRACT_SUCCESS=1
) else (
    REM Method 2: Download portable 7-Zip first
    echo 7-Zip not found. Downloading portable 7-Zip...
    powershell -Command "Invoke-WebRequest -Uri 'https://www.7-zip.org/a/7zr.exe' -OutFile '%FFMPEG_DIR%\7zr.exe'"
    
    if exist "%FFMPEG_DIR%\7zr.exe" (
        echo Extracting with portable 7-Zip...
        "%FFMPEG_DIR%\7zr.exe" x "%FFMPEG_DIR%\\ffmpeg.7z" -o"%FFMPEG_DIR%" -y
        del "%FFMPEG_DIR%\7zr.exe"
        set EXTRACT_SUCCESS=1
    ) else (
        set EXTRACT_SUCCESS=0
    )
)

if %EXTRACT_SUCCESS% neq 1 (
    echo ERROR: Failed to extract FFmpeg 7z file
    echo Falling back to ZIP version...
    REM Fallback to ZIP version
    del "%FFMPEG_DIR%\ffmpeg.7z"
    powershell -Command "Invoke-WebRequest -Uri 'https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip' -OutFile '%FFMPEG_DIR%\ffmpeg.zip'"
    powershell -Command "Expand-Archive -Path '%FFMPEG_DIR%\ffmpeg.zip' -DestinationPath '%FFMPEG_DIR%' -Force"
    del "%FFMPEG_DIR%\ffmpeg.zip"
) else (
    REM Clean up 7z file
    del "%FFMPEG_DIR%\ffmpeg.7z"
)

REM Find and move extracted folder
for /d %%i in ("%FFMPEG_DIR%\ffmpeg-*") do (
    xcopy "%%i\*" "%FFMPEG_DIR%" /E /I /Y
    rmdir "%%i" /S /Q
)

echo Adding FFmpeg to PATH...
if %ADMIN_MODE% == 1 (
    setx PATH "%PATH%;%FFMPEG_DIR%\bin" /M
    echo Added to system PATH (all users)
) else (
    setx PATH "%PATH%;%FFMPEG_DIR%\bin"
    echo Added to user PATH (current user only)
)

echo.
echo ========================================
echo Installation completed successfully!
echo ========================================
echo FFmpeg installed to: %FFMPEG_DIR%
echo Download size saved: ~70MB (7z vs ZIP)
echo.

REM Test installation
echo Testing installation...
"%FFMPEG_DIR%\bin\ffmpeg.exe" -version | findstr "ffmpeg version"

@REM pause