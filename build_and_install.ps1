$ErrorActionPreference = "Stop"
$root = "C:\Users\luec-a\Documents\CapCom"
$web = Join-Path $root "captain-web"
$cli = Join-Path $root "cli"
$ninja = Join-Path $root ".venv\Scripts\ninja.exe"
$gcc = "C:\TDM-GCC-64\bin\gcc.exe"
$gxx = "C:\TDM-GCC-64\bin\g++.exe"

Stop-Process -Name captain-web -Force -ErrorAction SilentlyContinue

Push-Location $web
cmake --preset default
cmake --build --preset default --clean-first
Pop-Location

$cliBuild = Join-Path $cli "build\gui"
cmake -S $cli -B $cliBuild -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_C_COMPILER=$gcc `
  -DCMAKE_CXX_COMPILER=$gxx `
  -DCMAKE_MAKE_PROGRAM=$ninja `
  -DCAPCOM_BUILD_TESTS=OFF
cmake --build $cliBuild --clean-first

$newCap = Join-Path $cliBuild "cap.exe"
$venvCap = Join-Path $root ".venv\Scripts\cap.exe"
if (-not (Test-Path $newCap)) { throw "Neue cap.exe wurde nicht erzeugt: $newCap" }
if (Test-Path $venvCap) { Copy-Item $venvCap "$venvCap.before-gui.bak" -Force }
Copy-Item $newCap $venvCap -Force
Write-Host "Webserver und echte Captain CLI wurden gebaut."
Write-Host "Neue CLI installiert: $venvCap"
