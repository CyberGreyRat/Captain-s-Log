param([int]$Port = 8080)
$ErrorActionPreference = "Stop"
$project = (Get-Location).Path
if (-not (Test-Path (Join-Path $project "reqs"))) { throw "Kein Captain-Projekt: $project" }
$root = "C:\Users\luec-a\Documents\CapCom\captain-web"
$server = Join-Path $root "build\debug\captain-web.exe"
$web = Join-Path $root "web"
$cap = "C:\Users\luec-a\Documents\CapCom\.venv\Scripts\cap.exe"
Stop-Process -Name captain-web -Force -ErrorAction SilentlyContinue
$bytes = New-Object byte[] 32
[Security.Cryptography.RandomNumberGenerator]::Fill($bytes)
$ticket = [Convert]::ToHexString($bytes)
$process = Start-Process $server -ArgumentList @($project,$web,$cap,$ticket,$Port) -PassThru
Start-Sleep -Milliseconds 800
Start-Process "http://127.0.0.1:$Port/auth/cap-gui?ticket=$ticket"
Write-Host "Captain's Log Developer session: $project"
Write-Host "Server PID: $($process.Id)"
