$ErrorActionPreference = "Stop"

$root = "C:\Users\luec-a\Documents\CapCom"
$source = Join-Path $root "Publish-CaptainsLog-WordReport-v4.ps1"
$final = Join-Path $root "Publish-CaptainsLog-WordReport-Final.ps1"
$wrapper = Join-Path $root ".venv\Scripts\Publish-CaptainsLog-Professional.ps1"

foreach ($requiredFile in @($source, $wrapper)) {
    if (-not (Test-Path -LiteralPath $requiredFile)) {
        throw "Required file not found: $requiredFile"
    }
}

Stop-Process -Name WINWORD -Force -ErrorAction SilentlyContinue

$text = [System.IO.File]::ReadAllText(
    $source,
    [System.Text.Encoding]::UTF8
)

function Replace-Required {
    param(
        [string]$Text,
        [string]$OldText,
        [string]$NewText,
        [string]$Description
    )

    if (-not $Text.Contains($OldText)) {
        throw "Patch point not found: $Description"
    }
    return $Text.Replace($OldText, $NewText)
}

$text = Replace-Required `
    -Text $text `
    -OldText '$word=New-Object -ComObject Word.Application' `
    -NewText 'Write-Host "[word 1/14] Starting Microsoft Word..."; $word=New-Object -ComObject Word.Application' `
    -Description "Word start"

$text = Replace-Required `
    -Text $text `
    -OldText '$word.Visible=$false;$word.DisplayAlerts=0' `
    -NewText '$word.Visible=$false;$word.DisplayAlerts=0;$word.ScreenUpdating=$false;$word.Options.CheckSpellingAsYouType=$false;$word.Options.CheckGrammarAsYouType=$false;$word.Options.SaveInterval=0;$word.Options.Pagination=$false' `
    -Description "Word settings"

$text = Replace-Required `
    -Text $text `
    -OldText '$document=$word.Documents.Add()' `
    -NewText 'Write-Host "[word 2/14] Creating document..."; $document=$word.Documents.Add()' `
    -Description "Document creation"

$progressPatches = @(
    @(
        "    Add-Heading `$sel '1. Management Summary' 1",
        "    Write-Host `"[word 3/14] Management summary...`"; Add-Heading `$sel '1. Management Summary' 1"
    ),
    @(
        "    Add-Heading `$sel '2. Traceability-Baum' 1",
        "    Write-Host `"[word 4/14] Traceability tree...`"; Add-Heading `$sel '2. Traceability-Baum' 1"
    ),
    @(
        "    Add-Heading `$sel '3. Traceability-Matrix' 1",
        "    Write-Host `"[word 5/14] Traceability matrix...`"; Add-Heading `$sel '3. Traceability-Matrix' 1"
    ),
    @(
        "    Add-Heading `$sel '4. Klassifizierung und Risiken' 1",
        "    Write-Host `"[word 6/14] Classification and risks...`"; Add-Heading `$sel '4. Klassifizierung und Risiken' 1"
    ),
    @(
        "    Add-Heading `$sel '5. Anforderungen im Detail' 1",
        "    Write-Host `"[word 7/14] Requirement details...`"; Add-Heading `$sel '5. Anforderungen im Detail' 1"
    ),
    @(
        "    Add-Heading `$sel '6. Test- und Verifikationsnachweise' 1",
        "    Write-Host `"[word 8/14] Test evidence...`"; Add-Heading `$sel '6. Test- und Verifikationsnachweise' 1"
    ),
    @(
        "    Add-Heading `$sel '7. Audit- und Berichtsnachweise' 1",
        "    Write-Host `"[word 9/14] Audit evidence...`"; Add-Heading `$sel '7. Audit- und Berichtsnachweise' 1"
    )
)

foreach ($patch in $progressPatches) {
    $text = Replace-Required `
        -Text $text `
        -OldText $patch[0] `
        -NewText $patch[1] `
        -Description $patch[0]
}

$pathOld = '$pdf=Join-Path $OutputDirectory "captains-log-traceability-$stamp.pdf"'
$pathNew = @'
$pdf=Join-Path $OutputDirectory "captains-log-traceability-$stamp.pdf"
$finalDocx = $docx
$finalPdf = $pdf
$localWork = Join-Path $env:TEMP ("captains-log-word-" + [Guid]::NewGuid().ToString("N"))
New-Item -Path $localWork -ItemType Directory -Force | Out-Null
$docx = Join-Path $localWork "report.docx"
$pdf = Join-Path $localWork "report.pdf"
'@
$text = Replace-Required `
    -Text $text `
    -OldText $pathOld `
    -NewText $pathNew.TrimEnd() `
    -Description "Local temporary paths"

$saveOld = @'
    $document.SaveAs2($docx,16)
    $document.ExportAsFixedFormat($pdf,17)
'@
$saveNew = @'
    Write-Host "[word 10/14] Saving DOCX to local temporary storage..."
    $saveArguments = [object[]]@([string]$docx,[int]16)
    $document.GetType().InvokeMember(
        "SaveAs2",
        [System.Reflection.BindingFlags]::InvokeMethod,
        $null,
        $document,
        $saveArguments,
        [System.Globalization.CultureInfo]::InvariantCulture
    ) | Out-Null
    Write-Host "[word 11/14] Local DOCX saved. PDF export deferred."
'@
$text = Replace-Required `
    -Text $text `
    -OldText $saveOld.Trim() `
    -NewText $saveNew.Trim() `
    -Description "DOCX save and PDF export block"

$checkOld = 'if(-not(Test-Path $pdf)){throw ''PDF was not created.''}'
$checkNew = @'
if (-not (Test-Path -LiteralPath $docx)) {
    throw "Local DOCX was not created: $docx"
}

Write-Host "[word 12/14] Starting fresh Word process for PDF export..."
$pdfWord = $null
$pdfDocument = $null
try {
    $pdfWord = New-Object -ComObject Word.Application
    $pdfWord.Visible = $false
    $pdfWord.DisplayAlerts = 0
    $pdfWord.ScreenUpdating = $false
    $pdfDocument = $pdfWord.Documents.Open([string]$docx,$false,$true,$false)

    Write-Host "[word 13/14] Exporting reopened DOCX to PDF..."
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
    if ($pdfDocument) { $pdfDocument.Close(0) }
    if ($pdfWord) { $pdfWord.Quit() }
    [GC]::Collect()
    [GC]::WaitForPendingFinalizers()
}

if (-not (Test-Path -LiteralPath $pdf)) {
    throw "Local PDF was not created: $pdf"
}

Write-Host "[word post] Moving completed files to reports..."
Copy-Item -LiteralPath $docx -Destination $finalDocx -Force
Copy-Item -LiteralPath $pdf -Destination $finalPdf -Force
Remove-Item -LiteralPath $localWork -Recurse -Force -ErrorAction SilentlyContinue
$docx = $finalDocx
$pdf = $finalPdf
'@
$text = Replace-Required `
    -Text $text `
    -OldText $checkOld `
    -NewText $checkNew.TrimEnd() `
    -Description "Separate PDF export"

# Use UTF-8 with BOM so Windows PowerShell 5.1 preserves umlauts.
[System.IO.File]::WriteAllText(
    $final,
    $text,
    [System.Text.UTF8Encoding]::new($true)
)

$tokens = $null
$parseErrors = $null
[System.Management.Automation.Language.Parser]::ParseFile(
    $final,
    [ref]$tokens,
    [ref]$parseErrors
) | Out-Null
if ($parseErrors.Count -gt 0) {
    $messages = $parseErrors | ForEach-Object {
        "Line $($_.Extent.StartLineNumber): $($_.Message)"
    }
    throw "Final publisher syntax errors:`n$($messages -join "`n")"
}

$wrapperBackup = "$wrapper.before-restored-final-v2.bak"
Copy-Item -LiteralPath $wrapper -Destination $wrapperBackup -Force
$wrapperText = [System.IO.File]::ReadAllText(
    $wrapper,
    [System.Text.Encoding]::UTF8
)
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

Write-Host "Restored final publisher with all stability fixes."
Write-Host "Final publisher: $final"
Write-Host "Wrapper backup: $wrapperBackup"
Write-Host "Run cap publish again."
