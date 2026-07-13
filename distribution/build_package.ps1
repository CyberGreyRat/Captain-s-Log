$ErrorActionPreference = "Stop"

$DistributionRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $DistributionRoot
$SourceExe = Join-Path $ProjectRoot "cli\build\debug\cap.exe"
$TargetExe = Join-Path $DistributionRoot "src\capcom\bin\cap.exe"

if (-not (Test-Path $SourceExe)) {
    throw "Compiled cap.exe not found: $SourceExe"
}

Copy-Item $SourceExe $TargetExe -Force
Write-Host "Copied cap.exe to package data."

Push-Location $DistributionRoot
try {
    python -m pip install --force-reinstall .
}
finally {
    Pop-Location
}

Write-Host "Installation complete. Open a new terminal if necessary and run: cap help"
