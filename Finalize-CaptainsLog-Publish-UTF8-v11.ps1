$ErrorActionPreference = "Stop"

$root = "C:\Users\luec-a\Documents\CapCom"
$source = Join-Path $root "Publish-CaptainsLog-WordReport-v9.ps1"
$final = Join-Path $root "Publish-CaptainsLog-WordReport-Final.ps1"
$wrapper = Join-Path $root ".venv\Scripts\Publish-CaptainsLog-Professional.ps1"

foreach ($requiredFile in @($source, $wrapper)) {
    if (-not (Test-Path -LiteralPath $requiredFile)) {
        throw "Required file not found: $requiredFile"
    }
}

Stop-Process -Name WINWORD -Force -ErrorAction SilentlyContinue

# v9 already contains the intended Unicode text. The problem is that Windows
# PowerShell 5.1 reads UTF-8 scripts without a BOM as an ANSI code page.
# Read v9 explicitly as UTF-8 and write the final script with a UTF-8 BOM.
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

$wrapperBackup = "$wrapper.before-utf8-v11.bak"
Copy-Item -LiteralPath $wrapper -Destination $wrapperBackup -Force

$wrapperText = [System.IO.File]::ReadAllText(
    $wrapper,
    [System.Text.Encoding]::UTF8
)
if ($wrapperText.Contains("Publish-CaptainsLog-WordReport-v9.ps1")) {
    $wrapperText = $wrapperText.Replace(
        "Publish-CaptainsLog-WordReport-v9.ps1",
        "Publish-CaptainsLog-WordReport-Final.ps1"
    )
}
elseif (-not $wrapperText.Contains("Publish-CaptainsLog-WordReport-Final.ps1")) {
    throw "The installed wrapper references neither v9 nor the final publisher."
}

[System.IO.File]::WriteAllText(
    $wrapper,
    $wrapperText,
    [System.Text.UTF8Encoding]::new($true)
)

$finalBytes = [System.IO.File]::ReadAllBytes($final)
if ($finalBytes.Length -lt 3 -or
    $finalBytes[0] -ne 0xEF -or
    $finalBytes[1] -ne 0xBB -or
    $finalBytes[2] -ne 0xBF) {
    throw "The final publisher does not contain a UTF-8 BOM."
}

Write-Host "Final UTF-8 publisher installed successfully."
Write-Host "Editable publisher: $final"
Write-Host "Wrapper backup: $wrapperBackup"
Write-Host "No C++ rebuild is required. Run cap publish again."
