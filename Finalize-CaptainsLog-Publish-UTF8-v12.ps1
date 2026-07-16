$ErrorActionPreference = "Stop"

$root = "C:\Users\luec-a\Documents\CapCom"
$wrapper = Join-Path $root ".venv\Scripts\Publish-CaptainsLog-Professional.ps1"
$final = Join-Path $root "Publish-CaptainsLog-WordReport-Final.ps1"

if (-not (Test-Path -LiteralPath $wrapper)) {
    throw "Installed wrapper not found: $wrapper"
}

$wrapperText = [System.IO.File]::ReadAllText(
    $wrapper,
    [System.Text.Encoding]::UTF8
)

$source = $null
$match = [System.Text.RegularExpressions.Regex]::Match(
    $wrapperText,
    'Publish-CaptainsLog-WordReport-(?:v[0-9]+|Final)\.ps1'
)
if ($match.Success) {
    $candidate = Join-Path $root $match.Value
    if (Test-Path -LiteralPath $candidate) {
        $source = $candidate
    }
}

if (-not $source) {
    $source = Get-ChildItem -LiteralPath $root -File `
        -Filter "Publish-CaptainsLog-WordReport-v*.ps1" |
        Sort-Object {
            if ($_.BaseName -match '-v([0-9]+)$') { [int]$matches[1] }
            else { -1 }
        } -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}

if (-not $source -or -not (Test-Path -LiteralPath $source)) {
    $available = Get-ChildItem -LiteralPath $root -File `
        -Filter "Publish-CaptainsLog-WordReport*.ps1" |
        Select-Object -ExpandProperty Name
    throw "No Word report publisher was found. Available: $($available -join ', ')"
}

Stop-Process -Name WINWORD -Force -ErrorAction SilentlyContinue

# Read explicitly as UTF-8 and write with a UTF-8 BOM for Windows PowerShell 5.1.
$text = [System.IO.File]::ReadAllText(
    $source,
    [System.Text.Encoding]::UTF8
)
[System.IO.File]::WriteAllText(
    $final,
    $text,
    [System.Text.UTF8Encoding]::new($true)
)

$tokens = $null
$errors = $null
[System.Management.Automation.Language.Parser]::ParseFile(
    $final,
    [ref]$tokens,
    [ref]$errors
) | Out-Null
if ($errors.Count -gt 0) {
    $messages = $errors | ForEach-Object {
        "Line $($_.Extent.StartLineNumber): $($_.Message)"
    }
    throw "Final publisher syntax check failed:`n$($messages -join "`n")"
}

$wrapperBackup = "$wrapper.before-utf8-v12.bak"
Copy-Item -LiteralPath $wrapper -Destination $wrapperBackup -Force

$wrapperText = [System.Text.RegularExpressions.Regex]::Replace(
    $wrapperText,
    'Publish-CaptainsLog-WordReport-(?:v[0-9]+|Final)\.ps1',
    'Publish-CaptainsLog-WordReport-Final.ps1'
)
[System.IO.File]::WriteAllText(
    $wrapper,
    $wrapperText,
    [System.Text.UTF8Encoding]::new($true)
)

$bytes = [System.IO.File]::ReadAllBytes($final)
if ($bytes.Length -lt 3 -or
    $bytes[0] -ne 0xEF -or
    $bytes[1] -ne 0xBB -or
    $bytes[2] -ne 0xBF) {
    throw "The final publisher does not contain a UTF-8 BOM."
}

Write-Host "Final UTF-8 publisher installed successfully."
Write-Host "Source publisher: $source"
Write-Host "Editable publisher: $final"
Write-Host "Wrapper backup: $wrapperBackup"
Write-Host "No C++ rebuild is required. Run cap publish again."
