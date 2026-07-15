param([int]$Port = 8080)
$ErrorActionPreference = "Stop"
$project = (Get-Location).Path
if (-not (Test-Path (Join-Path $project "reqs"))) { throw "Kein Captain-Projekt: $project" }
$root = "C:\Users\luec-a\Documents\CapCom\captain-web"
$server = Join-Path $root "build\debug\captain-web.exe"
$web = Join-Path $root "web"
$cap = "C:\Users\luec-a\Documents\CapCom\.venv\Scripts\cap.exe"
$bytes = New-Object byte[] 32
$rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
try {
    $rng.GetBytes($bytes)
}
finally {
    $rng.Dispose()
}
$ticket = ([System.BitConverter]::ToString($bytes)).Replace("-", "")
$process = $arguments = @(
    ('"' + $project + '"'),
    ('"' + $web + '"'),
    ('"' + $cap + '"'),
    ('"' + $ticket + '"'),
    ([string]$Port)
)
$process = Start-Process $server -ArgumentList $arguments -PassThru
Start-Sleep -Milliseconds 700
Start-Process "http://127.0.0.1:$Port/auth/cap-gui?ticket=$ticket"
Write-Host "Captain's Log Developer-Sitzung gestartet: $project"
Write-Host "Serverprozess: $($process.Id)"
