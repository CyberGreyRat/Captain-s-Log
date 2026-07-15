$ErrorActionPreference = "Stop"

$source = "C:\Users\luec-a\Documents\CapCom\captain-web"
$destination = "C:\Users\luec-a\Documents\Captain-Web-Current-Source.zip"

if (-not (Test-Path $source)) {
    throw "Captain Web source directory not found: $source"
}

Stop-Process -Name captain-web -Force -ErrorAction SilentlyContinue
Remove-Item $destination -Force -ErrorAction SilentlyContinue

$temporary = Join-Path $env:TEMP "captain-web-source-package"
Remove-Item $temporary -Recurse -Force -ErrorAction SilentlyContinue
New-Item $temporary -ItemType Directory | Out-Null

$exclude = @(
    "build",
    "vcpkg_installed",
    ".git",
    "captain-management.db",
    "build-output.txt"
)

Get-ChildItem $source -Force | Where-Object {
    $exclude -notcontains $_.Name
} | ForEach-Object {
    Copy-Item $_.FullName $temporary -Recurse -Force
}

Compress-Archive -Path "$temporary\*" -DestinationPath $destination -CompressionLevel Optimal
Remove-Item $temporary -Recurse -Force

Write-Host "Created source package: $destination"
Write-Host "Upload this ZIP in the chat."
