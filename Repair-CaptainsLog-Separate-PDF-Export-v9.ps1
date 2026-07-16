$ErrorActionPreference = "Stop"

$root = "C:\Users\luec-a\Documents\CapCom"
$v8 = Join-Path $root "Publish-CaptainsLog-WordReport-v8.ps1"
$v9 = Join-Path $root "Publish-CaptainsLog-WordReport-v9.ps1"
$wrapper = Join-Path $root ".venv\Scripts\Publish-CaptainsLog-Professional.ps1"

foreach ($required in @($v8, $wrapper)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required file not found: $required"
    }
}

Stop-Process -Name WINWORD -Force -ErrorAction SilentlyContinue

$text = [System.IO.File]::ReadAllText($v8, [System.Text.Encoding]::UTF8)

# Do not export PDF while the large report document is still being assembled.
# Save DOCX, close the first Word instance, then open the finished DOCX in a
# fresh Word process and export it to PDF.
$exportPattern = '(?s)\s*Write-Host "\[word 11/12\] Exporting PDF\.\.\.".*?Write-Host "\[word 12/12\] PDF exported\."'
$deferred = @'
    Write-Host "[word 11/12] PDF export deferred until Word is restarted."
'@
$updated = [System.Text.RegularExpressions.Regex]::Replace(
    $text,
    $exportPattern,
    "`n" + $deferred.TrimEnd(),
    1
)
if ($updated -eq $text) {
    throw "The in-process PDF export block was not found in WordReport-v8."
}
$text = $updated

$checkNeedle = 'if (-not (Test-Path -LiteralPath $docx)) { throw "Local DOCX was not created: $docx" }'
$separateExport = @'
Write-Host "[word 12a/14] Starting a fresh Word process for PDF export..."
$pdfWord = $null
$pdfDocument = $null
try {
    $pdfWord = New-Object -ComObject Word.Application
    $pdfWord.Visible = $false
    $pdfWord.DisplayAlerts = 0
    $pdfWord.ScreenUpdating = $false
    $pdfWord.Options.CheckSpellingAsYouType = $false
    $pdfWord.Options.CheckGrammarAsYouType = $false

    Write-Host "[word 12b/14] Opening the saved DOCX read-only..."
    $pdfDocument = $pdfWord.Documents.Open(
        [string]$docx,
        $false,
        $true,
        $false
    )

    Write-Host "[word 13/14] Exporting the reopened DOCX to PDF..."
    $pdfDocument.ExportAsFixedFormat(
        [string]$pdf,
        17,
        $false,
        0,
        0,
        1,
        9999,
        0,
        $true,
        $true,
        1,
        $true,
        $true,
        $false
    )
    Write-Host "[word 14/14] Separate PDF export completed."
}
finally {
    if ($pdfDocument) {
        $pdfDocument.Close(0)
    }
    if ($pdfWord) {
        $pdfWord.Quit()
    }
    [GC]::Collect()
    [GC]::WaitForPendingFinalizers()
}

if (-not (Test-Path -LiteralPath $docx)) { throw "Local DOCX was not created: $docx" }
'@
if (-not $text.Contains($checkNeedle)) {
    throw "The post-Word DOCX check was not found in WordReport-v8."
}
$text = $text.Replace($checkNeedle, $separateExport.TrimEnd())

[System.IO.File]::WriteAllText(
    $v9,
    $text,
    [System.Text.UTF8Encoding]::new($false)
)

$tokens = $null
$errors = $null
[System.Management.Automation.Language.Parser]::ParseFile(
    $v9,
    [ref]$tokens,
    [ref]$errors
) | Out-Null
if ($errors.Count -gt 0) {
    $messages = $errors | ForEach-Object {
        "Line $($_.Extent.StartLineNumber): $($_.Message)"
    }
    throw "WordReport-v9 syntax errors:`n$($messages -join "`n")"
}

$wrapperText = [System.IO.File]::ReadAllText($wrapper, [System.Text.Encoding]::UTF8)
if (-not $wrapperText.Contains("Publish-CaptainsLog-WordReport-v8.ps1")) {
    throw "Installed wrapper does not reference WordReport-v8."
}
Copy-Item -LiteralPath $wrapper -Destination "$wrapper.before-word-v9.bak" -Force
$wrapperText = $wrapperText.Replace(
    "Publish-CaptainsLog-WordReport-v8.ps1",
    "Publish-CaptainsLog-WordReport-v9.ps1"
)
[System.IO.File]::WriteAllText(
    $wrapper,
    $wrapperText,
    [System.Text.UTF8Encoding]::new($false)
)

Write-Host "Separate PDF export workaround v9 installed."
Write-Host "New publisher: $v9"
Write-Host "The DOCX is now closed before a fresh Word process exports the PDF."
Write-Host "Run cap publish again."
