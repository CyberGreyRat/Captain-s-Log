$ErrorActionPreference = "Stop"

$root = "C:\Users\luec-a\Documents\CapCom"
$source = Join-Path $root "Publish-CaptainsLog-WordReport-v9.ps1"
$final = Join-Path $root "Publish-CaptainsLog-WordReport-Final.ps1"
$wrapper = Join-Path $root ".venv\Scripts\Publish-CaptainsLog-Professional.ps1"

foreach ($required in @($source, $wrapper)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required file not found: $required"
    }
}

Stop-Process -Name WINWORD -Force -ErrorAction SilentlyContinue

$text = [System.IO.File]::ReadAllText($source, [System.Text.Encoding]::UTF8)

# Repair common UTF-8 text that was previously interpreted as Windows-1252.
$replacements = [ordered]@{
    'REQUIREMENTS Â· RISK Â· VERIFICATION' = 'REQUIREMENTS · RISK · VERIFICATION'
    'BegrÃ¼ndung' = 'Begründung'
    'Audit-EintrÃ¤ge' = 'Audit-Einträge'
    'SignaturprÃ¼fung' = 'Signaturprüfung'
    'gÃ¼ltig' = 'gültig'
    'GefÃ¤hrdung' = 'Gefährdung'
    'GefÃ¤hrdungssituation' = 'Gefährdungssituation'
    'AnfÃ¤nglich' = 'Anfänglich'
    'VerknÃ¼pfte' = 'Verknüpfte'
    'PrÃ¼fung' = 'Prüfung'
    'Ã¼' = 'ü'
    'Ã¤' = 'ä'
    'Ã¶' = 'ö'
    'ÃŸ' = 'ß'
}
foreach ($entry in $replacements.GetEnumerator()) {
    $text = $text.Replace($entry.Key, $entry.Value)
}

# PowerShell 5.1 needs a UTF-8 BOM to interpret script literals reliably.
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
    throw "Final publisher syntax errors:`n$($messages -join "`n")"
}

$wrapperText = [System.IO.File]::ReadAllText($wrapper, [System.Text.Encoding]::UTF8)
if (-not $wrapperText.Contains('Publish-CaptainsLog-WordReport-v9.ps1')) {
    throw "Installed wrapper does not reference WordReport-v9."
}
Copy-Item -LiteralPath $wrapper -Destination "$wrapper.before-final-v10.bak" -Force
$wrapperText = $wrapperText.Replace(
    'Publish-CaptainsLog-WordReport-v9.ps1',
    'Publish-CaptainsLog-WordReport-Final.ps1'
)
[System.IO.File]::WriteAllText(
    $wrapper,
    $wrapperText,
    [System.Text.UTF8Encoding]::new($true)
)

Write-Host "Final UTF-8 publisher installed."
Write-Host "Editable report script: $final"
Write-Host "The integration wrapper now uses the final script."
Write-Host "Run cap publish and check Begruendung, Audit-Eintraege and Signaturpruefung."
