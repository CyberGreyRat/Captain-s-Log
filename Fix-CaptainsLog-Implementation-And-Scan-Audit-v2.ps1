$ErrorActionPreference = "Stop"

$publisher = "C:\Users\luec-a\Documents\CapCom\Publish-CaptainsLog-WordReport-Final.ps1"
if (-not (Test-Path -LiteralPath $publisher)) {
    throw "Publisher not found: $publisher"
}

$backup = "$publisher.before-scan-evidence-v2.bak"
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
            $current = $null
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

function Find-CaptainsLogHistory {
    param([string]$RequirementFile)

    $directory = Split-Path $RequirementFile -Parent
    while ($directory) {
        $candidate = Join-Path $directory 'captainslog.yml'
        if (Test-Path -LiteralPath $candidate) { return $candidate }
        $parent = Split-Path $directory -Parent
        if (-not $parent -or $parent -eq $directory) { break }
        $directory = $parent
    }
    return $null
}

function Read-ImplementationScanAudit {
    param([string]$HistoryPath, [string]$Uid)

    $result = @()
    if (-not $HistoryPath -or -not (Test-Path -LiteralPath $HistoryPath)) {
        return $result
    }

    $entry = [ordered]@{}
    foreach ($line in [System.IO.File]::ReadAllLines($HistoryPath, [System.Text.Encoding]::UTF8)) {
        if ($line -match '^\s*-\s*uid:\s*(.*)$') {
            if ($entry.Count -gt 0 -and
                $entry.uid -eq $Uid -and
                $entry.action -eq 'IMPLEMENTATION_SCANNED') {
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

    if ($entry.Count -gt 0 -and
        $entry.uid -eq $Uid -and
        $entry.action -eq 'IMPLEMENTATION_SCANNED') {
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

    $historyPath = Find-CaptainsLogHistory $Item.file
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

# Replace the existing helper if present. Otherwise insert it before the next
# stable helper function. This patch does not depend on the spelling of $reqs.
$existingPattern = '(?s)function Add-ImplementationEvidence\s*\{.*?\r?\n\}\s*\r?\n(?=function (ConvertTo-XmlText|Set-SummaryCell))'
if ([regex]::IsMatch($text, $existingPattern)) {
    $text = [regex]::Replace($text, $existingPattern, $helpers, 1)
}
else {
    $marker = if ($text.Contains('function ConvertTo-XmlText {')) {
        'function ConvertTo-XmlText {'
    }
    elseif ($text.Contains('function Set-SummaryCell {')) {
        'function Set-SummaryCell {'
    }
    else { $null }

    if (-not $marker) { throw "Helper insertion point not found." }
    $text = $text.Replace($marker, $helpers + $marker)
}

# Insert calls into the requirement and test loops if they are not already
# present near the relevant section boundaries.
function Add-EvidenceCallBeforeBoundary {
    param([string]$InputText, [string]$Boundary, [int]$LookBack)

    $boundaryPosition = $InputText.IndexOf($Boundary)
    if ($boundaryPosition -lt 0) { throw "Section boundary not found: $Boundary" }

    $prefix = $InputText.Substring(0, $boundaryPosition)
    $near = $prefix.Substring([Math]::Max(0, $prefix.Length - $LookBack))
    if ($near.Contains('Add-ImplementationEvidence $selection $item')) {
        return $InputText
    }

    $loopEnd = $prefix.LastIndexOf("    }`r`n`r`n")
    $newline = "`r`n"
    if ($loopEnd -lt 0) {
        $loopEnd = $prefix.LastIndexOf("    }`n`n")
        $newline = "`n"
    }
    if ($loopEnd -lt 0) { throw "Loop end not found before: $Boundary" }

    return $InputText.Insert(
        $loopEnd,
        "        Add-ImplementationEvidence `$selection `$item$newline"
    )
}

$text = Add-EvidenceCallBeforeBoundary `
    -InputText $text `
    -Boundary "    Write-Host '[word 8/14] Test evidence...'" `
    -LookBack 3500

$text = Add-EvidenceCallBeforeBoundary `
    -InputText $text `
    -Boundary "    Write-Host '[word 9/14] Audit evidence...'" `
    -LookBack 2800

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
    throw "Patched publisher has syntax errors; backup restored:`n$($messages -join "`n")"
}

$check = [System.IO.File]::ReadAllText($publisher)
foreach ($requiredToken in @(
    'Read-ImplementationEvidenceFromYaml',
    'Find-CaptainsLogHistory',
    'Read-ImplementationScanAudit',
    'IMPLEMENTATION_SCANNED',
    'Scan-Historie'
)) {
    if (-not $check.Contains($requiredToken)) {
        throw "Post-patch verification failed: $requiredToken. Backup: $backup"
    }
}

Write-Host "Implementation and scan-audit evidence v2 installed."
Write-Host "No requirements-root patch was needed."
Write-Host "Backup: $backup"
Write-Host "No C++ rebuild is required. Run cap publish again."
