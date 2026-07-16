$ErrorActionPreference = "Stop"

$root = "C:\Users\luec-a\Documents\CapCom"
$source = Join-Path $root "Publish-CaptainsLog-WordReport-v4.ps1"
$final = Join-Path $root "Publish-CaptainsLog-WordReport-Final.ps1"
$wrapper = Join-Path $root ".venv\Scripts\Publish-CaptainsLog-Professional.ps1"

foreach ($required in @($source, $wrapper)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required file not found: $required"
    }
}

Stop-Process -Name WINWORD -Force -ErrorAction SilentlyContinue

$text = [System.IO.File]::ReadAllText($source, [System.Text.Encoding]::UTF8)

function Replace-Required {
    param([string]$InputText,[string]$Old,[string]$New,[string]$Name)
    if (-not $InputText.Contains($Old)) { throw "Patch point not found: $Name" }
    return $InputText.Replace($Old,$New)
}

# Progress and Word performance settings.
$text = Replace-Required $text '$word=New-Object -ComObject Word.Application' 'Write-Host "[word 1/14] Starting Microsoft Word..."; $word=New-Object -ComObject Word.Application' 'Word start'
$text = Replace-Required $text '$word.Visible=$false;$word.DisplayAlerts=0' '$word.Visible=$false;$word.DisplayAlerts=0;$word.ScreenUpdating=$false;$word.Options.CheckSpellingAsYouType=$false;$word.Options.CheckGrammarAsYouType=$false;$word.Options.SaveInterval=0;$word.Options.Pagination=$false' 'Word settings'
$text = Replace-Required $text '$document=$word.Documents.Add()' 'Write-Host "[word 2/14] Creating document..."; $document=$word.Documents.Add()' 'Document creation'

$progress = [ordered]@{
    "    Add-Heading `$sel '1. Management Summary' 1" = "    Write-Host \"[word 3/14] Management summary...\"; Add-Heading `$sel '1. Management Summary' 1"
    "    Add-Heading `$sel '2. Traceability-Baum' 1" = "    Write-Host \"[word 4/14] Traceability tree...\"; Add-Heading `$sel '2. Traceability-Baum' 1"
    "    Add-Heading `$sel '3. Traceability-Matrix' 1" = "    Write-Host \"[word 5/14] Traceability matrix...\"; Add-Heading `$sel '3. Traceability-Matrix' 1"
    "    Add-Heading `$sel '4. Klassifizierung und Risiken' 1" = "    Write-Host \"[word 6/14] Classification and risks...\"; Add-Heading `$sel '4. Klassifizierung und Risiken' 1"
    "    Add-Heading `$sel '5. Anforderungen im Detail' 1" = "    Write-Host \"[word 7/14] Requirement details...\"; Add-Heading `$sel '5. Anforderungen im Detail' 1"
    "    Add-Heading `$sel '6. Test- und Verifikationsnachweise' 1" = "    Write-Host \"[word 8/14] Test evidence...\"; Add-Heading `$sel '6. Test- und Verifikationsnachweise' 1"
    "    Add-Heading `$sel '7. Audit- und Berichtsnachweise' 1" = "    Write-Host \"[word 9/14] Audit evidence...\"; Add-Heading `$sel '7. Audit- und Berichtsnachweise' 1"
}
foreach ($entry in $progress.GetEnumerator()) {
    $text = Replace-Required $text $entry.Key $entry.Value "Progress $($entry.Key)"
}

# Save into local TEMP first.
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
$text = Replace-Required $text $pathOld $pathNew.TrimEnd() 'Local TEMP paths'

# Replace the original save and in-process PDF export with reliable reflected SaveAs2.
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
$text = Replace-Required $text $saveOld.Trim() $saveNew.Trim() 'SaveAs2/PDF block'

# After the first Word instance closes, reopen the finished DOCX in a fresh process.
$checkOld = 'if(-not(Test-Path $pdf)){throw ''PDF was not created.''}'
$checkNew = @'
if (-not (Test-Path -LiteralPath $docx)) { throw "Local DOCX was not created: $docx" }
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
    $pdfDocument.ExportAsFixedFormat([string]$pdf,17,$false,0,0,1,9999,0,$true,$true,1,$true,$true,$false)
    Write-Host "[word 14/14] Separate PDF export completed."
}
finally {
    if ($pdfDocument) { $pdfDocument.Close(0) }
    if ($pdfWord) { $pdfWord.Quit() }
    [GC]::Collect()
    [GC]::WaitForPendingFinalizers()
}
if (-not (Test-Path -LiteralPath $pdf)) { throw "Local PDF was not created: $pdf" }
Write-Host "[word post] Moving completed files to reports..."
Copy-Item -LiteralPath $docx -Destination $finalDocx -Force
Copy-Item -LiteralPath $pdf -Destination $finalPdf -Force
Remove-Item -LiteralPath $localWork -Recurse -Force -ErrorAction SilentlyContinue
$docx = $finalDocx
$pdf = $finalPdf
'@
$text = Replace-Required $text $checkOld $checkNew.TrimEnd() 'Separate PDF export'

# Save final script with UTF-8 BOM for Windows PowerShell 5.1.
[System.IO.File]::WriteAllText($final,$text,[System.Text.UTF8Encoding]::new($true))

$tokens=$null;$errors=$null
[System.Management.Automation.Language.Parser]::ParseFile($final,[ref]$tokens,[ref]$errors)|Out-Null
if($errors.Count -gt 0){$messages=$errors|ForEach-Object{"Line $($_.Extent.StartLineNumber): $($_.Message)"};throw "Final publisher syntax errors:`n$($messages -join "`n")"}

$backup="$wrapper.before-restored-final.bak";Copy-Item -LiteralPath $wrapper -Destination $backup -Force
$wrapperText=[System.IO.File]::ReadAllText($wrapper,[System.Text.Encoding]::UTF8)
$wrapperText=[regex]::Replace($wrapperText,'Publish-CaptainsLog-WordReport-(?:v[0-9]+|Final)\.ps1','Publish-CaptainsLog-WordReport-Final.ps1')
[System.IO.File]::WriteAllText($wrapper,$wrapperText,[System.Text.UTF8Encoding]::new($true))

Write-Host "Restored final publisher with all performance, save and PDF fixes."
Write-Host "Final publisher: $final"
Write-Host "Wrapper backup: $backup"
Write-Host "Run cap publish again."
