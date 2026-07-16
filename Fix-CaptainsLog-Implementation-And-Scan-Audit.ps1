$ErrorActionPreference = "Stop"

$publisher = "C:\Users\luec-a\Documents\CapCom\Publish-CaptainsLog-WordReport-Final.ps1"
if (-not (Test-Path -LiteralPath $publisher)) {
    throw "Publisher not found: $publisher"
}

$backup = "$publisher.before-scan-evidence.bak"
Copy-Item -LiteralPath $publisher -Destination $backup -Force
$text = [System.IO.File]::ReadAllText($publisher, [System.Text.Encoding]::UTF8)

$helpers = @'
function Read-ImplementationEvidenceFromYaml {
    param([string]$Path)

    $records = @()
    if (-not (Test-Path -LiteralPath $Path)) { return $records }

    $inside = $false
    $current = $null
    foreach ($line in [System.IO.File]::ReadAllLines($Path, [System.Text.Encoding]::UTF8)) {
        if ($line -match '^(implementation|implementations):\s*$') {
            $inside = $true
            continue
        }
        if ($inside -and $line -match '^[A-Za-z0-9_]+:\s*') {
            if ($current) { $records += [pscustomobject]$current }
            break
        }
        if (-not $inside) { continue }

        if ($line -match '^\s{2}-\s*([A-Za-z0-9_]+):\s*(.*)$') {
            if ($current) { $records += [pscustomobject]$current }
            $current = [ordered]@{}
            $current[$matches[1]] = Unquote-Value $matches[2]
            continue
        }
        if ($current -and $line -match '^\s{4}([A-Za-z0-9_]+):\s*(.*)$') {
            $current[$matches[1]] = Unquote-Value $matches[2]
        }
    }
    if ($current) { $records += [pscustomobject]$current }
    return $records
}

function Read-ImplementationScanAudit {
    param([string]$HistoryPath, [string]$Uid)

    $result = @()
    if (-not (Test-Path -LiteralPath $HistoryPath)) { return $result }

    $entry = [ordered]@{}
    foreach ($line in [System.IO.File]::ReadAllLines($HistoryPath, [System.Text.Encoding]::UTF8)) {
        if ($line -match '^\s*-\s*uid:\s*(.*)$') {
            if ($entry.Count -gt 0 -and $entry.uid -eq $Uid -and $entry.action -eq 'IMPLEMENTATION_SCANNED') {
                $result += [pscustomobject]$entry
            }
            $entry = [ordered]@{ uid = Unquote-Value $matches[1] }
            continue
        }
        if ($entry.Count -eq 0) { continue }
        if ($line -match '^\s{4,}(timestamp|action|actor|reason):\s*(.*)$') {
            $entry[$matches[1]] = Unquote-Value $matches[2]
        }
    }
    if ($entry.Count -gt 0 -and $entry.uid -eq $Uid -and $entry.action -eq 'IMPLEMENTATION_SCANNED') {
        $result += [pscustomobject]$entry
    }
    return $result
}

function Add-ImplementationEvidence {
    param($Selection, $Item)

    $implementations = @(Read-ImplementationEvidenceFromYaml $Item.file)
    if ($implementations.Count -eq 0) { return }

    Add-Heading $Selection 'Implementierungsnachweis' 4
    foreach ($implementation in $implementations) {
        $file = Join-DisplayValue $implementation.file
        $startLine = Join-DisplayValue $implementation.start_line
        $endLine = Join-DisplayValue $implementation.end_line
        $gitHash = Join-DisplayValue $implementation.git_hash
        $functionName = Join-DisplayValue $implementation.function
        if (-not $functionName) { $functionName = Join-DisplayValue $implementation.symbol }
        if (-not $functionName) { $functionName = Join-DisplayValue $implementation.name }

        $lineText = if ($startLine -and $endLine) {
            "$startLine-$endLine"
        }
        elseif ($startLine) { $startLine }
        else { 'Nicht angegeben' }

        $Selection.Font.Name = $Design.BodyFont
        $Selection.Font.Size = 10
        $Selection.Font.Bold = 1
        $Selection.Font.Color = Convert-HexToWordColor '15803D'
        $Selection.ParagraphFormat.SpaceAfter = 3
        $Selection.TypeText([char]0x2713 + ' ' + $file + ':' + $lineText + ' | Git ' + $gitHash)
        $Selection.TypeParagraph()

        Add-KeyValuePairs $Selection @(
            @('Quelldatei', $file),
            @('Zeilen', $lineText),
            @('Funktion / Symbol', $(if ($functionName) { $functionName } else { 'Nicht vom Scanner ermittelt' })),
            @('Git-Revision', $(if ($gitHash) { $gitHash } else { 'Nicht angegeben' }))
        )
    }

    $historyPath = Join-Path (Split-Path $Item.file -Parent) '..\captainslog.yml'
    $historyPath = [System.IO.Path]::GetFullPath($historyPath)
    if (-not (Test-Path -LiteralPath $historyPath)) {
        $historyPath = Join-Path $script:RequirementsRoot 'captainslog.yml'
    }
    $events = @(Read-ImplementationScanAudit $historyPath $Item.uid)
    if ($events.Count -gt 0) {
        Add-Heading $Selection 'Scan-Historie' 4
        foreach ($event in $events) {
            Add-KeyValuePairs $Selection @(
                @('Zeitpunkt', $(Join-DisplayValue $event.timestamp)),
                @('Aktion', $(Join-DisplayValue $event.action)),
                @('Benutzer', $(Join-DisplayValue $event.actor)),
                @('Grund', $(Join-DisplayValue $event.reason))
            )
        }
    }
}

'@

# Replace an existing implementation helper, or insert before Set-SummaryCell.
$pattern = '(?s)function Add-ImplementationEvidence\s*\{.*?\n\}\s*\n(?=(function ConvertTo-XmlText|function Set-SummaryCell))'
if ([regex]::IsMatch($text, $pattern)) {
    $text = [regex]::Replace($text, $pattern, $helpers, 1)
}
else {
    $marker = 'function Set-SummaryCell {'
    if (-not $text.Contains($marker)) { throw "Helper insertion point not found." }
    $text = $text.Replace($marker, $helpers + $marker)
}

# Make the requirements root available to the audit helper.
if (-not $text.Contains('$script:RequirementsRoot = $reqs')) {
    $needle = "$reqs = Join-Path $Project 'reqs'"
    if (-not $text.Contains($needle)) { throw "Requirements root insertion point not found." }
    $text = $text.Replace($needle, $needle + "`r`n`$script:RequirementsRoot = `$reqs")
}

# Add evidence calls to both requirement and test loops if not already present.
$requirementBoundary = "    Write-Host '[word 8/14] Test evidence...'"
$requirementPosition = $text.IndexOf($requirementBoundary)
if ($requirementPosition -lt 0) { throw "Requirement section boundary not found." }
$requirementPrefix = $text.Substring(0, $requirementPosition)
if (-not $requirementPrefix.Substring([Math]::Max(0, $requirementPrefix.Length - 3000)).Contains('Add-ImplementationEvidence $selection $item')) {
    $loopEnd = $requirementPrefix.LastIndexOf("    }`r`n`r`n")
    if ($loopEnd -lt 0) { $loopEnd = $requirementPrefix.LastIndexOf("    }`n`n") }
    if ($loopEnd -lt 0) { throw "Requirement loop end not found." }
    $text = $text.Insert($loopEnd, "        Add-ImplementationEvidence `$selection `$item`r`n")
}

$testBoundary = "    Write-Host '[word 9/14] Audit evidence...'"
$testPosition = $text.IndexOf($testBoundary)
if ($testPosition -lt 0) { throw "Test section boundary not found." }
$testPrefix = $text.Substring(0, $testPosition)
if (-not $testPrefix.Substring([Math]::Max(0, $testPrefix.Length - 2500)).Contains('Add-ImplementationEvidence $selection $item')) {
    $loopEnd = $testPrefix.LastIndexOf("    }`r`n`r`n")
    if ($loopEnd -lt 0) { $loopEnd = $testPrefix.LastIndexOf("    }`n`n") }
    if ($loopEnd -lt 0) { throw "Test loop end not found." }
    $text = $text.Insert($loopEnd, "        Add-ImplementationEvidence `$selection `$item`r`n")
}

[System.IO.File]::WriteAllText($publisher, $text, [System.Text.UTF8Encoding]::new($true))

$tokens = $null
$errors = $null
[System.Management.Automation.Language.Parser]::ParseFile($publisher, [ref]$tokens, [ref]$errors) | Out-Null
if ($errors.Count -gt 0) {
    Copy-Item -LiteralPath $backup -Destination $publisher -Force
    $messages = $errors | ForEach-Object { "Line $($_.Extent.StartLineNumber): $($_.Message)" }
    throw "Patched publisher has syntax errors; backup restored:`n$($messages -join "`n")"
}

Write-Host "Implementation and scan-audit evidence installed."
Write-Host "The publisher reads the singular YAML key implementation:."
Write-Host "Backup: $backup"
Write-Host "No C++ rebuild is required. Run cap publish again."
