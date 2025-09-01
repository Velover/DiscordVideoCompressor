# FFmpeg Installation Script (PowerShell Version)
# Requires Administrator privileges

param(
    [switch]$Force = $false
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "FFmpeg Installation Script (PowerShell)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# Check if running as administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")
if (-not $isAdmin) {
    Write-Host "ERROR: This script requires administrator privileges!" -ForegroundColor Red
    Write-Host "Please run PowerShell as Administrator or use elevated execution." -ForegroundColor Yellow
    exit 1
}

# Check if FFmpeg is already installed
try {
    $null = Get-Command ffmpeg -ErrorAction Stop
    $version = (& ffmpeg -version 2>$null | Select-String "ffmpeg version" | Select-Object -First 1).ToString()
    if (-not $Force) {
        Write-Host "FFmpeg is already installed:" -ForegroundColor Green
        Write-Host $version -ForegroundColor Gray
        Read-Host "Press Enter to exit"
        exit 0
    } else {
        Write-Host "FFmpeg found, but Force flag specified. Reinstalling..." -ForegroundColor Yellow
    }
} catch {
    Write-Host "FFmpeg not found. Starting installation..." -ForegroundColor Yellow
}

# Installation configuration
$ffmpegDir = "C:\Program Files\FFmpeg"
$ffmpegUrl = "https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.7z"
$zipFile = Join-Path $ffmpegDir "ffmpeg.7z"

Write-Host "Installing to: $ffmpegDir (System-wide)" -ForegroundColor Green

# Create installation directory
try {
    if (-not (Test-Path $ffmpegDir)) {
        Write-Host "Creating directory: $ffmpegDir"
        New-Item -ItemType Directory -Path $ffmpegDir -Force | Out-Null
    }
} catch {
    Write-Host "ERROR: Failed to create installation directory" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}

# Download FFmpeg
Write-Host "`nDownloading FFmpeg (7z format - ~30MB)..." -ForegroundColor Cyan
Write-Host "This may take a few minutes depending on your connection..."

try {
    $progressPreference = 'SilentlyContinue'
    Invoke-WebRequest -Uri $ffmpegUrl -OutFile $zipFile -UseBasicParsing
    Write-Host "Download completed successfully" -ForegroundColor Green
} catch {
    Write-Host "ERROR: Failed to download FFmpeg" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}

# Extract FFmpeg
Write-Host "`nExtracting FFmpeg..." -ForegroundColor Cyan

$extractSuccess = $false

# Method 1: Try system 7-Zip
$sevenZipPaths = @(
    "C:\Program Files\7-Zip\7z.exe",
    "C:\Program Files (x86)\7-Zip\7z.exe"
)

foreach ($path in $sevenZipPaths) {
    if (Test-Path $path) {
        try {
            Write-Host "Using installed 7-Zip: $path"
            & $path x $zipFile "-o$ffmpegDir" -y | Out-Null
            $extractSuccess = $true
            Write-Host "Extraction completed with system 7-Zip" -ForegroundColor Green
            break
        } catch {
            Write-Host "System 7-Zip failed, trying alternative..." -ForegroundColor Yellow
        }
    }
}

# Method 2: Download portable 7-Zip
if (-not $extractSuccess) {
    try {
        Write-Host "Downloading portable 7-Zip extractor..."
        $portable7z = Join-Path $ffmpegDir "7zr.exe"
        Invoke-WebRequest -Uri "https://www.7-zip.org/a/7zr.exe" -OutFile $portable7z -UseBasicParsing
        
        Write-Host "Extracting with portable 7-Zip..."
        & $portable7z x $zipFile "-o$ffmpegDir" -y | Out-Null
        Remove-Item $portable7z -Force
        $extractSuccess = $true
        Write-Host "Extraction completed with portable 7-Zip" -ForegroundColor Green
    } catch {
        Write-Host "Portable 7-Zip failed, trying ZIP fallback..." -ForegroundColor Yellow
    }
}

# Method 3: Fallback to ZIP
if (-not $extractSuccess) {
    try {
        Write-Host "Downloading ZIP version (larger file)..."
        Remove-Item $zipFile -Force
        $zipUrl = "https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip"
        $zipFile = Join-Path $ffmpegDir "ffmpeg.zip"
        Invoke-WebRequest -Uri $zipUrl -OutFile $zipFile -UseBasicParsing
        
        Write-Host "Extracting ZIP version..."
        Expand-Archive -Path $zipFile -DestinationPath $ffmpegDir -Force
        $extractSuccess = $true
        Write-Host "Extraction completed with ZIP" -ForegroundColor Green
    } catch {
        Write-Host "ERROR: All extraction methods failed" -ForegroundColor Red
        Write-Host $_.Exception.Message -ForegroundColor Red
        exit 1
    }
}

# Clean up archive
Remove-Item $zipFile -Force -ErrorAction SilentlyContinue

# Organize extracted files
Write-Host "`nOrganizing extracted files..." -ForegroundColor Cyan
$extractedFolder = Get-ChildItem -Path $ffmpegDir -Directory | Where-Object { $_.Name -like "ffmpeg-*" } | Select-Object -First 1

if ($extractedFolder) {
    Write-Host "Moving files from $($extractedFolder.Name) to root directory"
    try {
        Get-ChildItem -Path $extractedFolder.FullName -Recurse | Move-Item -Destination $ffmpegDir -Force
        Remove-Item $extractedFolder.FullName -Recurse -Force
        Write-Host "Files organized successfully" -ForegroundColor Green
    } catch {
        Write-Host "Warning: Some files may not have been moved correctly" -ForegroundColor Yellow
    }
} else {
    Write-Host "No extracted folder found - files may already be in correct location"
}

# Add to system PATH
Write-Host "`nAdding FFmpeg to system PATH..." -ForegroundColor Cyan
try {
    $currentPath = [Environment]::GetEnvironmentVariable("Path", [EnvironmentVariableTarget]::Machine)
    $ffmpegBinPath = Join-Path $ffmpegDir "bin"
    
    if ($currentPath -notlike "*$ffmpegBinPath*") {
        $newPath = "$currentPath;$ffmpegBinPath"
        [Environment]::SetEnvironmentVariable("Path", $newPath, [EnvironmentVariableTarget]::Machine)
        Write-Host "Added to system PATH successfully" -ForegroundColor Green
    } else {
        Write-Host "FFmpeg is already in system PATH" -ForegroundColor Yellow
    }
} catch {
    Write-Host "Warning: Failed to update system PATH automatically" -ForegroundColor Yellow
    Write-Host "Please add '$ffmpegDir\bin' to your system PATH manually" -ForegroundColor Yellow
}

# Test installation
Write-Host "`nTesting FFmpeg installation..." -ForegroundColor Cyan
try {
    $ffmpegExe = Join-Path $ffmpegDir "bin\ffmpeg.exe"
    if (Test-Path $ffmpegExe) {
        $version = & $ffmpegExe -version 2>$null | Select-String "ffmpeg version" | Select-Object -First 1
        Write-Host "FFmpeg test successful!" -ForegroundColor Green
        Write-Host $version -ForegroundColor Gray
    } else {
        Write-Host "Warning: ffmpeg.exe not found at expected location" -ForegroundColor Yellow
    }
} catch {
    Write-Host "Warning: FFmpeg test failed" -ForegroundColor Yellow
    Write-Host "The installation completed but FFmpeg may not be accessible yet" -ForegroundColor Yellow
}

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "Installation completed successfully!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "FFmpeg installed to: $ffmpegDir"
Write-Host "Installation type: System-wide (all users)"
Write-Host "Download size saved: ~70MB (7z vs ZIP)"
Write-Host "`nIMPORTANT: You may need to restart applications"
Write-Host "for PATH changes to take effect." -ForegroundColor Yellow

Write-Host "`nPress any key to exit..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
