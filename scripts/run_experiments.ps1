$ErrorActionPreference = 'Stop'
$mingw = "C:\Users\sumit\Downloads\winlibs-x86_64-posix-seh-gcc-16.2.0-mingw-w64ucrt-14.0.0-r3\mingw64\bin"
$env:Path = "$mingw;C:\Program Files\CMake\bin;C:\Users\sumit\bin\git\cmd;$env:Path"
Set-Location "C:\Users\sumit\OneDrive\Desktop\WaveBreak"

# 1. Find or build exe
$exe = $null
foreach ($candidate in @(".\build\spectrum_sentinel.exe",".\build\Release\spectrum_sentinel.exe")) {
    if (Test-Path $candidate) { $exe = $candidate; break }
}
if (-not $exe) {
    Write-Host "Exe not found -- configuring and building..." -ForegroundColor Yellow
    & cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
    & cmake --build build
    if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }
    foreach ($candidate in @(".\build\spectrum_sentinel.exe",".\build\Release\spectrum_sentinel.exe")) {
        if (Test-Path $candidate) { $exe = $candidate; break }
    }
    if (-not $exe) { throw "Exe still not found after build" }
}
Write-Host "Using exe: $exe" -ForegroundColor Green

# 2. Run evaluate (all 8 scenarios x 30 seeds)
Write-Host "Running evaluate (8 scenarios x 30 seeds)..." -ForegroundColor Cyan
& $exe evaluate
if ($LASTEXITCODE -ne 0) { throw "evaluate failed: $LASTEXITCODE" }

# 3. Print summary table from summary.csv
$csv = ".\results\summary.csv"
if (Test-Path $csv) {
    Write-Host "`n--- Summary Table ---" -ForegroundColor Cyan
    Import-Csv $csv | Format-Table -AutoSize
} else {
    Write-Warning "summary.csv not found at $csv"
}

Write-Host "run_experiments.ps1 complete." -ForegroundColor Green
