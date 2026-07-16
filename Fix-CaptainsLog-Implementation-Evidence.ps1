$ErrorActionPreference = "Stop"

$path = "C:\Users\luec-a\Documents\CapCom\Publish-CaptainsLog-WordReport-Final.ps1"
if (-not (Test-Path -LiteralPath $path)) {
    throw "Publisher not found: $path"
}

$backup = "$path.before-implementation-evidence-fix.bak"
Copy-Item -LiteralPath $path -Destination $backup -Force

$text = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)

# Captain CLI writes the YAML key as singular 'implementation'. Older report
# code expected plural 'implementations'. Accept both spellings, while storing
# all parsed records in the internal implementations array.
$text = $text.Replace(
    "@('integration_template','history','implementations','test')",
    "@('integration_template','history','implementation','implementations','test')"
)
$text = $text.Replace(
    "`$section -eq 'implementations'",
    "`$section -in @('implementation','implementations')"
)

if (-not $text.Contains("'implementation','implementations'")) {
    throw "The implementation section patch could not be applied."
}

[System.IO.File]::WriteAllText(
    $path,
    $text,
    [System.Text.UTF8Encoding]::new($true)
)

$tokens = $null
$errors = $null
[System.Management.Automation.Language.Parser]::ParseFile(
    $path,
    [ref]$tokens,
    [ref]$errors
) | Out-Null

if ($errors.Count -gt 0) {
    Copy-Item -LiteralPath $backup -Destination $path -Force
    $messages = $errors | ForEach-Object {
        "Line $($_.Extent.StartLineNumber): $($_.Message)"
    }
    throw "Patched publisher has syntax errors; backup restored:`n$($messages -join "`n")"
}

Write-Host "Implementation evidence parser fixed."
Write-Host "The report now accepts both implementation: and implementations:."
Write-Host "Backup: $backup"
Write-Host "Run cap publish again. No C++ rebuild is required."
