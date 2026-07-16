$ErrorActionPreference = "Stop"

$root = "C:\Users\luec-a\Documents\CapCom"
$publisher = Join-Path $root "Publish-CaptainsLog-WordReport-Final.ps1"

function Test-PowerShellFile {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return $false }
    $tokens = $null
    $errors = $null
    [System.Management.Automation.Language.Parser]::ParseFile(
        $Path,
        [ref]$tokens,
        [ref]$errors
    ) | Out-Null
    return $errors.Count -eq 0
}

# Find the newest syntactically valid version that still contains the compact
# tree and key/value report layout. The failed patch already restored its
# backup, but this also protects against an older corrupted working file.
$candidates = @(
    Get-ChildItem -LiteralPath $root -File -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Name -like "Publish-CaptainsLog-WordReport-Final*.ps1" -or
            $_.Name -like "Publish-CaptainsLog-WordReport-Final*.bak"
        } |
        Sort-Object LastWriteTime -Descending
)

$source = $null
foreach ($candidate in $candidates) {
    if (-not (Test-PowerShellFile $candidate.FullName)) { continue }
    $candidateText = [System.IO.File]::ReadAllText(
        $candidate.FullName,
        [System.Text.Encoding]::UTF8
    )
    if ($candidateText.Contains("function Add-KeyValuePairs") -and
        $candidateText.Contains("function Add-TreeNodeLine") -and
        $candidateText.Contains("[word 14/14]")) {
        $source = $candidate.FullName
        break
    }
}

if (-not $source) {
    throw "No valid compact Word publisher backup was found in $root."
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$safetyBackup = "$publisher.before-evidence-recovery-$timestamp.bak"
if (Test-Path -LiteralPath $publisher) {
    Copy-Item -LiteralPath $publisher -Destination $safetyBackup -Force
}
if ($source -ne $publisher) {
    Copy-Item -LiteralPath $source -Destination $publisher -Force
}

$text = [System.IO.File]::ReadAllText(
    $publisher,
    [System.Text.Encoding]::UTF8
)

$helper = @'
function Add-ImplementationAndScanEvidence {
    param($Selection, $Item)

    $implementations = @()
    $insideImplementation = $false
    $currentImplementation = $null

    foreach ($line in [System.IO.File]::ReadAllLines($Item.file, [System.Text.Encoding]::UTF8)) {
        if ($line -match '^(implementation|implementations):\s*$') {
            $insideImplementation = $true
            continue
        }
        if ($insideImplementation -and $line -match '^[A-Za-z0-9_]+:\s*') {
            if ($currentImplementation) {
                $implementations += [pscustomobject]$currentImplementation
            }
            $currentImplementation = $null
            break
        }
        if (-not $insideImplementation) { continue }

        if ($line -match '^\s{2}-\s*([A-Za-z0-9_]+):\s*(.*)$') {
            if ($currentImplementation) {
                $implementations += [pscustomobject]$currentImplementation
            }
            $currentImplementation = [ordered]@{}
            $currentImplementation[$matches[1]] = Unquote-Value $matches[2]
            continue
        }
        if ($currentImplementation -and $line -match '^\s{4}([A-Za-z0-9_]+):\s*(.*)$') {
            $currentImplementation[$matches[1]] = Unquote-Value $matches[2]
        }
    }
    if ($currentImplementation) {
        $implementations += [pscustomobject]$currentImplementation
    }

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
        if (-not $functionName) { $functionName = 'Nicht vom Scanner ermittelt' }

        $lineRange = if ($startLine -and $endLine) {
            "$startLine-$endLine"
        }
        elseif ($startLine) { $startLine }
        else { 'Nicht angegeben' }

        $Selection.Font.Name = $Design.BodyFont
        $Selection.Font.Size = 10
        $Selection.Font.Bold = 1
        $Selection.Font.Color = Convert-HexToWordColor '15803D'
        $Selection.ParagraphFormat.SpaceAfter = 3
        $Selection.TypeText(([char]0x2713) + " $file`:$lineRange | Git $gitHash")
        $Selection.TypeParagraph()

        Add-KeyValuePairs $Selection @(
            @('Quelldatei', $file),
            @('Zeilen', $lineRange),
            @('Funktion / Symbol', $functionName),
            @('Git-Revision', $(if ($gitHash) { $gitHash } else { 'Nicht angegeben' }))
        )
    }

    # Find reqs/captainslog.yml by walking upwards from the requirement file.
    $directory = Split-Path $Item.file -Parent
    $historyPath = $null
    while ($directory) {
        $candidate = Join-Path $directory 'captainslog.yml'
        if (Test-Path -LiteralPath $candidate) {
            $historyPath = $candidate
            break
        }
        $parent = Split-Path $directory -Parent
        if (-not $parent -or $parent -eq $directory) { break }
        $directory = $parent
    }

    if (-not $historyPath) { return }

    $events = @()
    $entry = [ordered]@{}
    foreach ($line in [System.IO.File]::ReadAllLines($historyPath, [System.Text.Encoding]::UTF8)) {
        if ($line -match '^\s*-\s*uid:\s*(.*)$') {
            if ($entry.Count -gt 0 -and
                $entry.uid -eq $Item.uid -and
                $entry.action -eq 'IMPLEMENTATION_SCANNED') {
                $events += [pscustomobject]$entry
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
        $entry.uid -eq $Item.uid -and
        $entry.action -eq 'IMPLEMENTATION_SCANNED') {
        $events += [pscustomobject]$entry
    }

    if ($events.Count -eq 0) { return }

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

'@

# Remove only our own previous helper if present, then insert one clean copy.
$text = [regex]::Replace(
    $text,
    '(?s)function Add-ImplementationAndScanEvidence\s*\{.*?\r?\n\}\s*\r?\n(?=function )',
    '',
    1
)

$insertMarker = 'function Set-SummaryCell {'
if (-not $text.Contains($insertMarker)) {
    throw "Stable helper insertion point not found in recovered publisher."
}
$text = $text.Replace($insertMarker, $helper + $insertMarker)

function Add-CallBeforeBoundary {
    param(
        [string]$InputText,
        [string]$Boundary,
        [int]$LookBack
    )

    $boundaryPosition = $InputText.IndexOf($Boundary)
    if ($boundaryPosition -lt 0) {
        throw "Section boundary not found: $Boundary"
    }

    $prefix = $InputText.Substring(0, $boundaryPosition)
    $near = $prefix.Substring([Math]::Max(0, $prefix.Length - $LookBack))
    if ($near.Contains('Add-ImplementationAndScanEvidence $selection $item')) {
        return $InputText
    }

    $loopEnd = $prefix.LastIndexOf("    }`r`n`r`n")
    $newline = "`r`n"
    if ($loopEnd -lt 0) {
        $loopEnd = $prefix.LastIndexOf("    }`n`n")
        $newline = "`n"
    }
    if ($loopEnd -lt 0) {
        throw "Loop end not found before: $Boundary"
    }

    return $InputText.Insert(
        $loopEnd,
        "        Add-ImplementationAndScanEvidence `$selection `$item$newline"
    )
}

$text = Add-CallBeforeBoundary `
    -InputText $text `
    -Boundary "    Write-Host '[word 8/14] Test evidence...'" `
    -LookBack 3500

$text = Add-CallBeforeBoundary `
    -InputText $text `
    -Boundary "    Write-Host '[word 9/14] Audit evidence...'" `
    -LookBack 2800

[System.IO.File]::WriteAllText(
    $publisher,
    $text,
    [System.Text.UTF8Encoding]::new($true)
)

if (-not (Test-PowerShellFile $publisher)) {
    Copy-Item -LiteralPath $source -Destination $publisher -Force
    throw "Final syntax validation failed. The valid source was restored: $source"
}

$finalText = [System.IO.File]::ReadAllText($publisher)
if (($finalText.Split('Add-ImplementationAndScanEvidence $selection $item').Count - 1) -lt 2) {
    Copy-Item -LiteralPath $source -Destination $publisher -Force
    throw "Evidence calls were not installed in both report sections. Valid source restored."
}

Write-Host "Word publisher recovered and evidence support installed."
Write-Host "Recovered from: $source"
Write-Host "Safety backup:  $safetyBackup"
Write-Host "No C++ build is required. Run cap publish again."
