# Build script for Video Compressor with dynamic Qt linking
param(
    [string]$BuildType = "Debug"
)

# Validate build type
if ($BuildType -ne "Release" -and $BuildType -ne "Debug") {
    Write-Host "Invalid build type. Use -Release or -Debug" -ForegroundColor Red
    Write-Host "Usage: .\build.ps1 [-BuildType Release|Debug]" -ForegroundColor Yellow
    exit 1
}

Write-Host "Building Video Compressor ($BuildType)..." -ForegroundColor Green

# Set Qt path for dynamic builds
$env:PATH = "C:\Qt\6.9.2\mingw_64\bin;" + $env:PATH

# Clean previous build and output
if (Test-Path ".\build") {
    Write-Host "Cleaning previous build..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force ".\build"
}

if (Test-Path ".\out") {
    Write-Host "Cleaning previous output..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force ".\out"
}

# Configure with CMake
Write-Host "Configuring CMake for $BuildType build..." -ForegroundColor Blue
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE="$BuildType"

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    exit 1
}

# Build the project
Write-Host "Building project..." -ForegroundColor Blue
cmake --build build --config $BuildType

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

# Create output directory and copy executable
Write-Host "Preparing output..." -ForegroundColor Blue
New-Item -ItemType Directory -Force -Path ".\out" | Out-Null
Copy-Item -Path ".\build\video_compressor.exe" -Destination ".\out" -Force

# Copy install-ffmpeg.bat script
Write-Host "Copying FFmpeg installer..." -ForegroundColor Blue
Copy-Item -Path ".\Scripts\install-ffmpeg.bat" -Destination ".\out" -Force

# Deploy Qt dependencies using windeployqt
Write-Host "Deploying Qt dependencies..." -ForegroundColor Blue
windeployqt out\video_compressor.exe --qmldir .\qml 2>&1 | Where-Object { 
    $_ -notmatch "^Checking" -and 
    $_ -notmatch "^Updating" -and
    $_ -notmatch "^Installing:" -and
    $_ -notmatch "^Creating" -and
    $_ -notmatch "^Running:" -and
    $_ -ne ""
}

if ($LASTEXITCODE -ne 0) {
    Write-Host "windeployqt failed!" -ForegroundColor Red
    exit 1
}

Write-Host "Build completed successfully!" -ForegroundColor Green
Write-Host "Build Type: $BuildType" -ForegroundColor Cyan
Write-Host "Executable location: .\out\video_compressor.exe" -ForegroundColor Cyan
Write-Host "FFmpeg installer: .\out\install-ffmpeg.bat" -ForegroundColor Cyan
Write-Host "All required DLLs have been deployed to the out directory." -ForegroundColor Cyan
