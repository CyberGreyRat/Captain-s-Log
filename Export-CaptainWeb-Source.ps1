$ErrorActionPreference = "Stop"

$root = "C:\Users\luec-a\Documents\CapCom\captain-web"
$output = "C:\Users\luec-a\Documents\Captain-Web-Current-Source.txt"

if (-not (Test-Path $root)) {
    throw "Captain Web directory not found: $root"
}

$extensions = @(".cpp", ".hpp", ".h", ".html", ".js", ".css", ".json", ".txt", ".md")
$explicitFiles = @("CMakeLists.txt", "CMakePresets.json")

$files = Get-ChildItem $root -Recurse -File | Where-Object {
    $relative = $_.FullName.Substring($root.Length).TrimStart("\")
    $top = ($relative -split "\\")[0]

    $top -notin @("build", "third_party", "sqlite-amalgamation", "vcpkg_installed") -and
    ($extensions -contains $_.Extension -or $explicitFiles -contains $_.Name)
} | Sort-Object FullName

$builder = [System.Text.StringBuilder]::new()
[void]$builder.AppendLine("Captain Web source export")
[void]$builder.AppendLine("Generated: $([DateTime]::UtcNow.ToString('o'))")
[void]$builder.AppendLine("Root: $root")
[void]$builder.AppendLine("")

foreach ($file in $files) {
    $relative = $file.FullName.Substring($root.Length).TrimStart("\")
    [void]$builder.AppendLine("===== FILE: $relative =====")
    [void]$builder.AppendLine([System.IO.File]::ReadAllText($file.FullName))
    [void]$builder.AppendLine("===== END FILE: $relative =====")
    [void]$builder.AppendLine("")
}

[System.IO.File]::WriteAllText(
    $output,
    $builder.ToString(),
    [System.Text.UTF8Encoding]::new($false)
)

Write-Host "Created: $output"
Write-Host "Files exported: $($files.Count)"
Write-Host "Upload the TXT file in the chat."
