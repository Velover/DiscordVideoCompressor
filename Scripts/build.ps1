cmake -S . -B build -G "MinGW Makefiles"
cmake --build build --config Release

$env:PATH = "C:\Qt\6.9.2\mingw_64\bin;" + $env:PATH

# Copy the deployed exe (and its copied DLLs) to /out
New-Item -ItemType Directory -Force -Path ".\out" | Out-Null
Copy-Item -Path ".\build\qt_hello.exe" -Destination ".\out" -Force
windeployqt out\qt_hello.exe
