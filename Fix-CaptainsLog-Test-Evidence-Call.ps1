$ErrorActionPreference = "Stop"

$publisher = "C:\Users\luec-a\Documents\CapCom\Publish-CaptainsLog-WordReport-Final.ps1"
if (-not (Test-Path -LiteralPath $publisher)) {
    throw "Publisher not found: $publisher"
}

$backup = "$publisher.before-ut-evidence-call.bak"
Copy-Item -LiteralPath $publisher -Destination $backup -Force
$text = [System.IO.File]::ReadAllText($publisher, [System.Text.Encoding]::UTF8)

$call = 'Add-ImplementationAndScanEvidence $selection $item'
$definition = 'function Add-ImplementationAndScanEvidence'
if (-not $text.Contains($definition)) {
    throw "Evidence helper is missing. Restore or run the recovery script first."
}

# Locate the test section and the following audit section.
$testStartMarker = "    Write-Host '[word 8/14] Test evidence...'"
$auditStartMarker = "    Write-Host '[word 9/14] Audit evidence...'"
$testStart = $text.IndexOf($testStartMarker)
$auditStart = $text.IndexOf($auditStartMarker)
if ($testStart -lt 0 -or $auditStart -le $testStart) {
    throw "Test or audit section boundary not found."
}

$testSection = $text.Substring($testStart, $auditStart - $testStart)
if (-not $testSection.Contains($call)) {
    # Insert immediately before the closing brace of the test foreach loop.
    $relativeLoopEnd = $testSection.LastIndexOf("    }`r`n")
    $newline = "`r`n"
    if ($relativeLoopEnd -lt 0) {
        $relativeLoopEnd = $testSection.LastIndexOf("    }`n")
        $newline = "`n"
    }
    if ($relativeLoopEnd -lt 0) {
        throw "Closing brace of the test loop not found."
    }

    $absoluteLoopEnd = $testStart + $relativeLoopEnd
    $insertion = "        $call$newline"
    $text = $text.Insert($absoluteLoopEnd, $insertion)
}

[System.IO.File]::WriteAllText(
    $publisher,
    $text,
    [System.Text.UTF8Encoding]::new($true)
)

$tokens = $null
$errors = $null
[System.Management.Automation.Language.Parser]::ParseFile(
    $publisher,
    [ref]$tokens,
    [ref]$errors
) | Out-Null

if ($errors.Count -gt 0) {
    Copy-Item -LiteralPath $backup -Destination $publisher -Force
    $messages = $errors | ForEach-Object {
        "Line $($_.Extent.StartLineNumber): $($_.Message)"
    }
    throw "Syntax check failed; backup restored:`n$($messages -join "`n")"
}

$finalText = [System.IO.File]::ReadAllText($publisher)
$occurrences = ([regex]::Matches(
    $finalText,
    [regex]::Escape($call)
)).Count

$testStart = $finalText.IndexOf($testStartMarker)
$auditStart = $finalText.IndexOf($auditStartMarker)
$finalTestSection = $finalText.Substring($testStart, $auditStart - $testStart)
if (-not $finalTestSection.Contains($call)) {
    Copy-Item -LiteralPath $backup -Destination $publisher -Force
    throw "The evidence call was not installed in the test section. Backup restored."
}

Write-Host "UT/test implementation evidence call installed."
Write-Host "Total helper calls found: $occurrences"
Write-Host "Expected total Select-String matches: 3 (definition + requirements + tests)."
Write-Host "Backup: $backup"
Write-Host "No C++ rebuild is required. Run cap publish again."
