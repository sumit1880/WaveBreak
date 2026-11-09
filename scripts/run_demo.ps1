$ErrorActionPreference = 'Stop'
$mingw = "C:\Users\sumit\Downloads\winlibs-x86_64-posix-seh-gcc-16.2.0-mingw-w64ucrt-14.0.0-r3\mingw64\bin"
$env:Path = "$mingw;C:\Program Files\CMake\bin;C:\Users\sumit\bin\git\cmd;$env:Path"
Set-Location "C:\Users\sumit\OneDrive\Desktop\WaveBreak"

# 1. Find or build exe
$exe = $null
foreach ($candidate in @(".\build\wavebreak.exe",".\build\Release\wavebreak.exe")) {
    if (Test-Path $candidate) { $exe = $candidate; break }
}
if (-not $exe) {
    Write-Host "Exe not found -- configuring and building..." -ForegroundColor Yellow
    & cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
    & cmake --build build
    if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }
    foreach ($candidate in @(".\build\wavebreak.exe",".\build\Release\wavebreak.exe")) {
        if (Test-Path $candidate) { $exe = $candidate; break }
    }
    if (-not $exe) { throw "Exe still not found after build" }
}
Write-Host "Using exe: $exe" -ForegroundColor Green

# 2. Run simulate narrowband
Write-Host "Running simulate narrowband..." -ForegroundColor Cyan
& $exe simulate --scenario narrowband
if ($LASTEXITCODE -ne 0) { throw "simulate failed: $LASTEXITCODE" }

# 3. Run demo
Write-Host "Running demo..." -ForegroundColor Cyan
& $exe demo
if ($LASTEXITCODE -ne 0) { throw "demo failed: $LASTEXITCODE" }

Write-Host "run_demo.ps1 complete." -ForegroundColor Green
