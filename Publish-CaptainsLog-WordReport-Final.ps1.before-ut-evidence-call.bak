param(
    [string]$Project = (Get-Location).Path,
    [string]$OutputDirectory = "",
    [switch]$Draft,
    [switch]$KeepDocx = $true
)

$ErrorActionPreference = "Stop"

# =============================================================================
# PERSONAL REPORT DESIGN
# Change values in this block to adjust the appearance of Word and PDF.
# Keep this file encoded as UTF-8 with BOM.
# =============================================================================
$Design = @{
    PrimaryColor = "0D3158"
    AccentColor = "D0A53A"
    MutedColor = "667085"
    LightColor = "EAF1F8"
    BodyFont = "Aptos"
    HeadingFont = "Aptos Display"
    BodySize = 10
    SmallSize = 8
    HeaderLogoWidthCm = 1.35
    PageMarginTopCm = 1.8
    PageMarginBottomCm = 1.7
    PageMarginLeftCm = 2.0
    PageMarginRightCm = 1.8
    Classification = "INTERNAL"
}

$ChapterDescriptions = @{
    Summary = "Dieses Kapitel fasst Projektumfang, Bearbeitungsstatus, Validierung, Audit-Integrität und die Verteilung der Anforderungsarten zusammen."
    Tree = "Der Traceability-Baum zeigt die fachliche Hierarchie von Nutzeranforderungen über System-, Software- und Sicherheitsanforderungen bis zu den Verifikationsnachweisen. Mehrfach dargestellte Einträge besitzen mehrere fachlich gültige Parents."
    Matrix = "Die Traceability-Übersicht dokumentiert alle direkten Parent- und Child-Beziehungen und unterstützt die Prüfung auf vollständige und widerspruchsfreie Nachweisketten."
    Risk = "Dieses Kapitel dokumentiert Software-Sicherheitsklassifizierung, Gefährdungen, Risikokontrollen und zugeordnete Verifikationstests."
    Requirements = "Dieses Kapitel enthält die vollständige Anforderungsspezifikation einschließlich Status, Version, Begründung, Verantwortlichkeiten und Traceability-Beziehungen."
    Tests = "Dieses Kapitel beschreibt Testziele, Testmethoden, erwartete Ergebnisse, Verantwortlichkeiten und die jeweils verifizierten Anforderungen."
    Audit = "Dieses Kapitel dokumentiert die signierte Änderungshistorie, die Git-Revision und die Integrität des veröffentlichten Berichtstands."
}

$TypeNames = @{
    CLASS = "Software Safety Classification"
    RISK = "Risk Management Record"
    USR = "User Requirement"
    SYS = "System Requirement"
    SRS = "Software Requirement Specification"
    SEC = "Security Requirement"
    ARCH = "Architecture Requirement"
    LLD = "Low-Level Design Requirement"
    UT = "Unit Test"
    IT = "Integration Test"
    ST = "System Test"
    AT = "Acceptance Test"
    TEST = "Test Specification"
    BUG = "Defect Record"
    CR = "Change Request"
    SOUP = "Software of Unknown Provenance"
}

$FieldLabels = @{
    owner = "Verantwortlich"
    classification_ref = "Klassifizierungsreferenz"
    classification_rationale = "Klassifizierungsbegründung"
    classification_responsible = "Klassifizierungsverantwortlich"
    software_item_safety_class = "Software-Sicherheitsklasse"
    software_safety_class = "Software-Sicherheitsklasse"
    software_system = "Softwaresystem"
    risk_references = "Risikoreferenzen"
    risk_management_document = "Risikomanagementdokument"
    risk_management_record_ids = "Risikomanagementeinträge"
    hazard = "Gefährdung"
    hazardous_situation = "Gefährdungssituation"
    harm = "Schaden"
    initial_probability = "Anfängliche Wahrscheinlichkeit"
    initial_severity = "Anfänglicher Schweregrad"
    risk_responsible = "Risikoverantwortlich"
    control_requirements = "Risikokontrollanforderungen"
    verification_tests = "Verifikationstests"
    verifies = "Verifizierte Anforderungen"
    test_method = "Testmethode"
    expected_result = "Erwartetes Ergebnis"
    test_responsible = "Testverantwortlich"
    external_document = "Externes Dokument"
    external_record_id = "Externe Datensatz-ID"
    external_sheet = "Externes Tabellenblatt"
    external_version = "Externe Version"
}

function Convert-HexToWordColor {
    param([string]$Hex)
    $red = [Convert]::ToInt32($Hex.Substring(0, 2), 16)
    $green = [Convert]::ToInt32($Hex.Substring(2, 2), 16)
    $blue = [Convert]::ToInt32($Hex.Substring(4, 2), 16)
    return $red + (256 * $green) + (65536 * $blue)
}

function Unquote-Value {
    param([string]$Value)
    $Value = $Value.Trim()
    if ($Value.Length -ge 2 -and $Value[0] -eq '"' -and $Value[$Value.Length - 1] -eq '"') {
        $Value = $Value.Substring(1, $Value.Length - 2)
        $Value = $Value.Replace('\n', "`n").Replace('\r', "`r").Replace('\t', "`t")
        $Value = $Value.Replace('\"', '"').Replace('\\', '\')
    }
    return $Value
}

function Parse-ListValue {
    param([string]$Value)
    $Value = $Value.Trim()
    if (-not ($Value.StartsWith('[') -and $Value.EndsWith(']'))) { return @() }
    $inner = $Value.Substring(1, $Value.Length - 2).Trim()
    if (-not $inner) { return @() }
    return @($inner.Split(',') | ForEach-Object { Unquote-Value $_ } | Where-Object { $_ })
}

function Read-RequirementFile {
    param([string]$Path)
    $item = [ordered]@{
        uid = ''; type = ''; status = ''; title = ''; text = ''; rationale = ''
        version = ''; git_hash = ''; attributes = [ordered]@{}; implementations = @()
        parents = @(); children = @(); file = $Path
    }
    $section = ''
    $currentImplementation = $null
    foreach ($raw in [System.IO.File]::ReadAllLines($Path, [System.Text.Encoding]::UTF8)) {
        if (-not $raw.Trim() -or $raw.TrimStart().StartsWith('#')) { continue }
        if ($raw -match '^([A-Za-z0-9_]+):\s*(.*)$') {
            $key = $matches[1]; $value = $matches[2]; $section = ''
            if ($key -eq 'attributes') { $section = 'attributes'; continue }
            if ($key -in @('integration_template','history','implementation','implementations','test')) { $section = $key; continue }
            if ($key -in @('parents','children')) { $item[$key] = Parse-ListValue $value }
            elseif ($item.Contains($key)) { $item[$key] = Unquote-Value $value }
            continue
        }
        if ($section -eq 'attributes' -and $raw -match '^\s{2}([A-Za-z0-9_]+):\s*(.*)$') {
            $key = $matches[1]; $value = $matches[2]
            if ($value.Trim().StartsWith('[')) { $item.attributes[$key] = @(Parse-ListValue $value) }
            else { $item.attributes[$key] = Unquote-Value $value }
        }
        elseif ($section -in @('implementation','implementations') -and $raw -match '^\s{2}-\s*([A-Za-z0-9_]+):\s*(.*)$') {
            if ($currentImplementation) { $item.implementations += [pscustomobject]$currentImplementation }
            $currentImplementation = [ordered]@{}
            $currentImplementation[$matches[1]] = Unquote-Value $matches[2]
        }
        elseif ($section -in @('implementation','implementations') -and $currentImplementation -and $raw -match '^\s{4}([A-Za-z0-9_]+):\s*(.*)$') {
            $currentImplementation[$matches[1]] = Unquote-Value $matches[2]
        }
    }
    if ($section -in @('implementation','implementations') -and $currentImplementation) { $item.implementations += [pscustomobject]$currentImplementation }
    return [pscustomobject]$item
}

function Join-DisplayValue {
    param($Value)
    if ($null -eq $Value) { return '' }
    if ($Value -is [System.Array]) { return ($Value -join ', ') }
    return [string]$Value
}

function Get-TypeName {
    param([string]$Type)
    if ($TypeNames.ContainsKey($Type)) { return $TypeNames[$Type] }
    return $Type
}

function Get-FieldLabel {
    param([string]$Key)
    if ($FieldLabels.ContainsKey($Key)) { return $FieldLabels[$Key] }
    $words = $Key.Replace('_', ' ')
    if (-not $words) { return $Key }
    return $words.Substring(0, 1).ToUpperInvariant() + $words.Substring(1)
}

function Add-Heading {
    param($Selection, [string]$Text, [int]$Level)
    $styleId = switch ($Level) { 1 { -2 } 2 { -3 } 3 { -4 } 4 { -5 } default { -1 } }
    $Selection.Style = $styleId
    $Selection.TypeText($Text)
    $Selection.TypeParagraph()
    $Selection.Style = -1
}

function Add-BodyParagraph {
    param($Selection, [string]$Text, [double]$SpaceAfter = 6)
    $Selection.Font.Name = $Design.BodyFont
    $Selection.Font.Size = $Design.BodySize
    $Selection.Font.Bold = 0
    $Selection.Font.Italic = 0
    $Selection.Font.Color = Convert-HexToWordColor '000000'
    $Selection.ParagraphFormat.LeftIndent = 0
    $Selection.ParagraphFormat.RightIndent = 0
    $Selection.ParagraphFormat.FirstLineIndent = 0
    $Selection.ParagraphFormat.SpaceBefore = 0
    $Selection.ParagraphFormat.SpaceAfter = $SpaceAfter
    $Selection.ParagraphFormat.LineSpacingRule = 0
    $Selection.ParagraphFormat.LineSpacing = 13
    $Selection.TypeText([string]$Text)
    $Selection.TypeParagraph()
}

function Add-ChapterDescription {
    param($Selection, [string]$Text)
    $Selection.Font.Name = $Design.BodyFont
    $Selection.Font.Size = 10
    $Selection.Font.Bold = 0
    $Selection.Font.Italic = 1
    $Selection.Font.Color = Convert-HexToWordColor $Design.MutedColor
    $Selection.ParagraphFormat.LeftIndent = 8
    $Selection.ParagraphFormat.RightIndent = 8
    $Selection.ParagraphFormat.SpaceBefore = 1
    $Selection.ParagraphFormat.SpaceAfter = 10
    $Selection.ParagraphFormat.LineSpacingRule = 0
    $Selection.ParagraphFormat.LineSpacing = 13
    $Selection.TypeText($Text)
    $Selection.TypeParagraph()
    $Selection.Font.Italic = 0
    $Selection.ParagraphFormat.LeftIndent = 0
    $Selection.ParagraphFormat.RightIndent = 0
}

function Add-KeyValuePairs {
    param($Selection, [array]$Rows)
    foreach ($row in $Rows) {
        $key = [string]$row[0]
        $value = [string]$row[1]
        if ([string]::IsNullOrWhiteSpace($value)) { $value = 'Nicht angegeben' }

        $Selection.Style = -1
        $Selection.Font.Name = $Design.BodyFont
        $Selection.Font.Size = 9
        $Selection.Font.Bold = 1
        $Selection.Font.Italic = 0
        $Selection.Font.Color = Convert-HexToWordColor $Design.PrimaryColor
        $Selection.ParagraphFormat.LeftIndent = 0
        $Selection.ParagraphFormat.FirstLineIndent = 0
        $Selection.ParagraphFormat.SpaceBefore = 0
        $Selection.ParagraphFormat.SpaceAfter = 3
        $Selection.ParagraphFormat.LineSpacingRule = 0
        $Selection.ParagraphFormat.LineSpacing = 12
        $Selection.TypeText($key + ': ')

        $Selection.Font.Size = 10
        $Selection.Font.Bold = 0
        $Selection.Font.Color = Convert-HexToWordColor '000000'
        $Selection.TypeText($value)
        $Selection.TypeParagraph()
    }
    $Selection.ParagraphFormat.SpaceAfter = 6
    $Selection.TypeParagraph()
}

function Add-TreeNodeLine {
    param($Selection, [int]$Level, $Item, [bool]$Reference)
    $Selection.Style = -1
    $Selection.Font.Name = $Design.BodyFont
    $Selection.Font.Size = 9
    $Selection.ParagraphFormat.LeftIndent = 18 * $Level
    $Selection.ParagraphFormat.FirstLineIndent = 0
    $Selection.ParagraphFormat.SpaceBefore = 0
    $Selection.ParagraphFormat.SpaceAfter = 0
    $Selection.ParagraphFormat.LineSpacingRule = 0
    $Selection.ParagraphFormat.LineSpacing = 10

    $symbol = if ($Level -eq 0) { '● ' } else { '└─ ' }
    $Selection.Font.Bold = 0
    $Selection.Font.Color = Convert-HexToWordColor $Design.AccentColor
    $Selection.TypeText($symbol)

    $Selection.Font.Bold = 1
    $Selection.Font.Color = Convert-HexToWordColor $Design.PrimaryColor
    $Selection.TypeText($Item.uid)

    $Selection.Font.Bold = 0
    $Selection.Font.Color = Convert-HexToWordColor $Design.MutedColor
    $Selection.TypeText('  [' + (Get-TypeName $Item.type) + ']  ')

    $Selection.Font.Color = Convert-HexToWordColor '000000'
    $Selection.TypeText($Item.title)

    $Selection.Font.Size = 8
    $Selection.Font.Color = Convert-HexToWordColor $Design.MutedColor
    $suffix = '  (' + $Item.status + ')'
    if ($Reference) { $suffix += '  [Referenz]' }
    $Selection.TypeText($suffix)
    $Selection.TypeParagraph()
}

function Add-TreeBranch {
    param($Selection, [string]$Uid, [int]$Level, [hashtable]$Path, [hashtable]$ItemsByUid)
    if (-not $ItemsByUid.ContainsKey($Uid)) { return }
    $item = $ItemsByUid[$Uid]
    $isReference = $Path.ContainsKey($Uid)
    Add-TreeNodeLine $Selection $Level $item $isReference
    if ($isReference) { return }
    $nextPath = @{} + $Path
    $nextPath[$Uid] = $true
    foreach ($child in $item.children) {
        Add-TreeBranch $Selection $child ($Level + 1) $nextPath $ItemsByUid
    }
}

function Add-ImplementationEvidence {
    param($Selection, $Item)
    if (-not $Item.implementations -or $Item.implementations.Count -eq 0) { return }
    Add-Heading $Selection 'Implementierungsnachweis' 4
    foreach ($implementation in $Item.implementations) {
        $file = Join-DisplayValue $implementation.file
        $startLine = Join-DisplayValue $implementation.start_line
        $endLine = Join-DisplayValue $implementation.end_line
        $functionName = Join-DisplayValue $implementation.function
        if (-not $functionName) { $functionName = Join-DisplayValue $implementation.symbol }
        if (-not $functionName) { $functionName = Join-DisplayValue $implementation.name }
        $gitHash = Join-DisplayValue $implementation.git_hash
        $lineText = if ($startLine -and $endLine) { "$startLine-$endLine" } elseif ($startLine) { $startLine } else { 'Nicht angegeben' }
        Add-KeyValuePairs $Selection @(
            @('Quelldatei', $file),
            @('Zeilen', $lineText),
            @('Funktion / Symbol', $functionName),
            @('Git-Revision', $gitHash)
        )
    }
}

function ConvertTo-XmlText {
    param([string]$Text)
    if ($null -eq $Text) { return '' }
    return [System.Security.SecurityElement]::Escape($Text)
}

function New-TraceabilitySvg {
    param([array]$Items, [hashtable]$ItemsByUid, [string]$OutputPath)
    $depths=@{}; $queue=New-Object System.Collections.Queue
    foreach($rootItem in @($Items|Where-Object{$_.parents.Count -eq 0})){$depths[$rootItem.uid]=0;$queue.Enqueue($rootItem.uid)}
    while($queue.Count -gt 0){$uid=[string]$queue.Dequeue();$depth=[int]$depths[$uid];if(-not $ItemsByUid.ContainsKey($uid)){continue};foreach($child in $ItemsByUid[$uid].children){if(-not $depths.ContainsKey($child)-or[int]$depths[$child]-lt($depth+1)){$depths[$child]=$depth+1;$queue.Enqueue($child)}}}
    $positions=@{};$rowsByDepth=@{};$maxDepth=0
    foreach($item in $Items){$depth=if($depths.ContainsKey($item.uid)){[int]$depths[$item.uid]}else{0};if($depth-gt$maxDepth){$maxDepth=$depth};if(-not$rowsByDepth.ContainsKey($depth)){$rowsByDepth[$depth]=0};$row=[int]$rowsByDepth[$depth];$rowsByDepth[$depth]=$row+1;$positions[$item.uid]=@{x=75+($depth*215);y=48+($row*78)}}
    $maxRows=1;foreach($value in $rowsByDepth.Values){if([int]$value-gt$maxRows){$maxRows=[int]$value}};$width=230+($maxDepth*215);$height=85+($maxRows*78)
    $builder=New-Object System.Text.StringBuilder
    [void]$builder.AppendLine("<svg xmlns='http://www.w3.org/2000/svg' width='$width' height='$height' viewBox='0 0 $width $height'><rect width='100%' height='100%' fill='white'/><defs><marker id='a' markerWidth='8' markerHeight='8' refX='7' refY='3' orient='auto'><path d='M0,0 L0,6 L8,3 z' fill='#98A2B3'/></marker></defs>")
    foreach($parent in $Items){foreach($child in $parent.children){if($positions.ContainsKey($parent.uid)-and$positions.ContainsKey($child)){$a=$positions[$parent.uid];$b=$positions[$child];[void]$builder.AppendLine("<line x1='$($a.x+22)' y1='$($a.y)' x2='$($b.x-22)' y2='$($b.y)' stroke='#98A2B3' stroke-width='2' marker-end='url(#a)'/>")}}}
    foreach($item in $Items){$p=$positions[$item.uid];$uid=ConvertTo-XmlText $item.uid;$title=ConvertTo-XmlText $item.title;$type=ConvertTo-XmlText (Get-TypeName $item.type);[void]$builder.AppendLine("<circle cx='$($p.x)' cy='$($p.y)' r='20' fill='#0D3158' stroke='#D0A53A' stroke-width='3'/><text x='$($p.x)' y='$($p.y+3)' text-anchor='middle' font-family='Arial' font-size='7' font-weight='bold' fill='white'>$uid</text><text x='$($p.x+28)' y='$($p.y-3)' font-family='Arial' font-size='10' font-weight='bold' fill='#0D3158'>$type</text><text x='$($p.x+28)' y='$($p.y+12)' font-family='Arial' font-size='8' fill='#344054'>$title</text>")}
    [void]$builder.AppendLine('</svg>');[System.IO.File]::WriteAllText($OutputPath,$builder.ToString(),[System.Text.UTF8Encoding]::new($false));return $builder.ToString()
}
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
function Set-SummaryCell {
    param($Cell, [string]$Text, [bool]$Bold = $false, [int]$Color = 0)
    $Cell.Range.Text = $Text
    $Cell.Range.Font.Name = $Design.BodyFont
    $Cell.Range.Font.Size = 9
    $Cell.Range.Font.Bold = [int]$Bold
    if ($Color -ne 0) { $Cell.Range.Font.Color = $Color }
    $Cell.VerticalAlignment = 1
    $Cell.Range.ParagraphFormat.SpaceBefore = 2
    $Cell.Range.ParagraphFormat.SpaceAfter = 2
}

function Shade-Cell {
    param($Cell, [string]$Hex)
    $Cell.Shading.BackgroundPatternColor = Convert-HexToWordColor $Hex
}

$Project = (Resolve-Path -LiteralPath $Project).Path
$reqs = Join-Path $Project 'reqs'
if (-not (Test-Path -LiteralPath $reqs)) { throw "No Captain project found: $Project" }
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) { $OutputDirectory = Join-Path $Project 'reports' }
elseif (-not [System.IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory = Join-Path $Project $OutputDirectory }
New-Item -Path $OutputDirectory -ItemType Directory -Force | Out-Null

$logo = 'C:\Users\luec-a\Documents\CapCom\captain-web\web\logo.png'
if (-not (Test-Path -LiteralPath $logo)) { throw "Logo not found: $logo" }
$cap = (Get-Command cap -ErrorAction Stop).Source

Push-Location $Project
try {
    & $cap validate | Out-Host
    $validationExitCode = $LASTEXITCODE
    & $cap verify | Out-Host
    $verificationExitCode = $LASTEXITCODE
}
finally { Pop-Location }
if (($validationExitCode -ne 0 -or $verificationExitCode -ne 0) -and -not $Draft) {
    throw 'Validation or verification failed. Use -Draft for a marked draft report.'
}

$typeOrder = @{ CLASS=0; RISK=1; USR=2; SYS=3; SRS=4; SEC=5; ARCH=6; LLD=7; UT=8; IT=9; ST=10; AT=11; TEST=12; BUG=13; CR=14; SOUP=15 }
$items = @(Get-ChildItem $reqs -Recurse -File -Include *.yaml,*.yml |
    Where-Object { $_.Name -notin @('captainslog.yml','captainslog.yaml') -and $_.FullName -notmatch '\\remote\\' } |
    ForEach-Object { Read-RequirementFile $_.FullName } |
    Where-Object { $_.uid } |
    Sort-Object @{ Expression = { if ($typeOrder.ContainsKey($_.type)) { $typeOrder[$_.type] } else { 99 } } }, uid)

$itemsByUid = @{}
foreach ($item in $items) { $itemsByUid[$item.uid] = $item }
$projectName = Split-Path $Project -Leaf
$generatedDate = Get-Date -Format 'dd.MM.yyyy'
$generatedDateTime = Get-Date -Format 'dd.MM.yyyy HH:mm'
$gitRevision = 'working-tree'
try {
    $gitResult = & git -C $Project rev-parse --short HEAD 2>$null
    if ($LASTEXITCODE -eq 0 -and $gitResult) { $gitRevision = $gitResult.Trim() }
}
catch {}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$finalDocx = Join-Path $OutputDirectory "captains-log-traceability-$stamp.docx"
$finalPdf = Join-Path $OutputDirectory "captains-log-traceability-$stamp.pdf"
$localWork = Join-Path $env:TEMP ("captains-log-word-" + [Guid]::NewGuid().ToString('N'))
New-Item -Path $localWork -ItemType Directory -Force | Out-Null
$docx = Join-Path $localWork 'report.docx'
$pdf = Join-Path $localWork 'report.pdf'

Write-Host '[word 1/14] Starting Microsoft Word...'
$word = $null; $document = $null
try {
    $word = New-Object -ComObject Word.Application
    $word.Visible = $false
    $word.DisplayAlerts = 0
    $word.ScreenUpdating = $false
    $word.Options.CheckSpellingAsYouType = $false
    $word.Options.CheckGrammarAsYouType = $false
    $word.Options.SaveInterval = 0
    $word.Options.Pagination = $false

    Write-Host '[word 2/14] Creating document...'
    $document = $word.Documents.Add()
    $section = $document.Sections.Item(1)
    $section.PageSetup.TopMargin = $word.CentimetersToPoints($Design.PageMarginTopCm)
    $section.PageSetup.BottomMargin = $word.CentimetersToPoints($Design.PageMarginBottomCm)
    $section.PageSetup.LeftMargin = $word.CentimetersToPoints($Design.PageMarginLeftCm)
    $section.PageSetup.RightMargin = $word.CentimetersToPoints($Design.PageMarginRightCm)
    $section.PageSetup.DifferentFirstPageHeaderFooter = -1

    $normal = $document.Styles.Item(-1)
    $normal.Font.Name = $Design.BodyFont
    $normal.Font.Size = $Design.BodySize
    foreach ($styleId in @(-2,-3,-4,-5)) {
        $style = $document.Styles.Item($styleId)
        $style.Font.Name = $Design.HeadingFont
        $style.Font.Color = Convert-HexToWordColor $Design.PrimaryColor
    }
    $document.Styles.Item(-2).Font.Size = 18
    $document.Styles.Item(-3).Font.Size = 13
    $document.Styles.Item(-4).Font.Size = 10
    $document.Styles.Item(-5).Font.Size = 9

    # Compact header from page 2 onwards. Avoid a header table because some
    # Word COM installations reject table creation inside a header range.
    Write-Host '[word 2a/14] Configuring header and footer...'
    try {
        $header = $section.Headers.Item(1).Range
        $header.Text = "Traceability Report - $projectName - $generatedDate`rCaptain's Log"
        $header.Paragraphs.Item(1).Range.Font.Name = $Design.HeadingFont
        $header.Paragraphs.Item(1).Range.Font.Size = 9
        $header.Paragraphs.Item(1).Range.Font.Bold = 1
        $header.Paragraphs.Item(1).Range.Font.Color = Convert-HexToWordColor $Design.PrimaryColor
        $header.Paragraphs.Item(1).Range.ParagraphFormat.SpaceAfter = 0
        $header.Paragraphs.Item(2).Range.Font.Name = $Design.BodyFont
        $header.Paragraphs.Item(2).Range.Font.Size = 7
        $header.Paragraphs.Item(2).Range.Font.Bold = 0
        $header.Paragraphs.Item(2).Range.Font.Color = Convert-HexToWordColor $Design.MutedColor
        $header.Paragraphs.Item(2).Range.ParagraphFormat.SpaceAfter = 0
$firstHeader = $section.Headers.Item(2).Range
        $firstHeader.Text = ''
    }
    catch {
        throw "Header creation failed: $($_.Exception.Message)"
    }

    try {
        $footer = $section.Footers.Item(1).Range
        $footer.Text = "$($Design.Classification)  |  Revision $gitRevision  |  Erzeugt am $generatedDate  |  Seite "
        $footer.Font.Name = $Design.BodyFont
        $footer.Font.Size = 8
        $footer.Font.Color = Convert-HexToWordColor $Design.MutedColor
        $footer.Collapse(0)
        $footer.Fields.Add($footer, -1, 'PAGE') | Out-Null
        $footer.InsertAfter(' von ')
        $footer.Collapse(0)
        $footer.Fields.Add($footer, -1, 'NUMPAGES') | Out-Null
    }
    catch {
        throw "Footer creation failed: $($_.Exception.Message)"
    }

    $selection = $word.Selection

    # Cover page
    Write-Host '[word 2b/14] Creating cover page...'
    $coverTable = $document.Tables.Add($selection.Range, 1, 2)
    $coverTable.Borders.Enable = 0
    $coverTable.Columns.Item(1).Width = $word.CentimetersToPoints(4.3)
    $coverTable.Columns.Item(2).Width = $word.CentimetersToPoints(12.0)
    $coverShape = $coverTable.Cell(1,1).Range.InlineShapes.AddPicture($logo, $false, $true)
    $coverShape.LockAspectRatio = -1
    $coverShape.Width = $word.CentimetersToPoints(3.8)
    $coverTable.Cell(1,2).Range.Text = "REQUIREMENTS · RISK · VERIFICATION`rCaptain's Log`rTraceability Report"
    foreach ($cellIndex in 1..2) { Shade-Cell $coverTable.Cell(1,$cellIndex) $Design.PrimaryColor }
    $coverTable.Cell(1,2).Range.Font.Color = Convert-HexToWordColor 'FFFFFF'
    $coverTable.Cell(1,2).Range.Font.Name = $Design.HeadingFont
    $coverTable.Cell(1,2).Range.Paragraphs.Item(1).Range.Font.Size = 8
    $coverTable.Cell(1,2).Range.Paragraphs.Item(2).Range.Font.Size = 27
    $coverTable.Cell(1,2).Range.Paragraphs.Item(2).Range.Font.Bold = 1
    $coverTable.Cell(1,2).Range.Paragraphs.Item(3).Range.Font.Size = 16
    $selection.SetRange($coverTable.Range.End, $coverTable.Range.End)
    $selection.TypeParagraph(); $selection.TypeParagraph()

    Add-KeyValuePairs $selection @(
        @('Projekt', $projectName),
        @('Berichtsstatus', $(if ($Draft) { 'DRAFT - nicht freigegeben' } else { 'Freigegebener Nachweisbericht' })),
        @('Erstellt', $generatedDateTime),
        @('Git-Revision', $gitRevision),
        @('Validierung', $(if ($validationExitCode -eq 0) { 'Erfolgreich' } else { 'Fehlgeschlagen' })),
        @('Audit-Signaturen', $(if ($verificationExitCode -eq 0) { 'Verifiziert' } else { 'Nicht verifiziert' }))
    )

    Add-Heading $selection 'Inhalt' 2
    $tocRange = $selection.Range
    $toc = $document.TablesOfContents.Add(
        $tocRange,
        $true,
        1,
        3,
        $false,
        '',
        $true,
        $true,
        '',
        $true,
        $true,
        $true
    )
    $selection.SetRange($toc.Range.End, $toc.Range.End)
    $selection.InsertBreak(7)
    Write-Host '[word 3/14] Management summary...'
    Add-Heading $selection '1. Management Summary' 1
    Add-ChapterDescription $selection $ChapterDescriptions.Summary
    $groups = @($items | Group-Object type | Sort-Object Name)
    $summaryTable = $document.Tables.Add($selection.Range, $groups.Count + 1, 3)
    $summaryTable.Borders.Enable = 1
    $summaryTable.Rows.Item(1).HeadingFormat = -1
    $summaryTable.TopPadding = 4; $summaryTable.BottomPadding = 4
    $summaryTable.LeftPadding = 5; $summaryTable.RightPadding = 5
    foreach ($column in 1..3) {
        Shade-Cell $summaryTable.Cell(1,$column) $Design.PrimaryColor
        Set-SummaryCell $summaryTable.Cell(1,$column) @('Anforderungsart','Anzahl','Status')[$column - 1] $true (Convert-HexToWordColor 'FFFFFF')
    }
    for ($index = 0; $index -lt $groups.Count; $index++) {
        Set-SummaryCell $summaryTable.Cell($index + 2,1) (Get-TypeName $groups[$index].Name) $true
        Set-SummaryCell $summaryTable.Cell($index + 2,2) ([string]$groups[$index].Count)
        Set-SummaryCell $summaryTable.Cell($index + 2,3) (($groups[$index].Group.status | Sort-Object -Unique) -join ', ')
        if ($index % 2 -eq 0) { foreach ($column in 1..3) { Shade-Cell $summaryTable.Cell($index + 2,$column) 'F7F9FC' } }
    }
    $selection.SetRange($summaryTable.Range.End, $summaryTable.Range.End)
    $selection.TypeParagraph()
    Add-BodyParagraph $selection "Gesamtobjekte: $($items.Count) | Validierung: $(if ($validationExitCode -eq 0) { 'erfolgreich' } else { 'fehlgeschlagen' }) | Audit: $(if ($verificationExitCode -eq 0) { 'verifiziert' } else { 'nicht verifiziert' })"

    Write-Host '[word 4/14] Traceability tree...'
    Add-Heading $selection '2. Traceability-Baum' 1
    Add-ChapterDescription $selection $ChapterDescriptions.Tree
    foreach ($rootItem in @($items | Where-Object { $_.parents.Count -eq 0 })) {
        Add-TreeBranch $selection $rootItem.uid 0 @{} $itemsByUid
    }
    $selection.ParagraphFormat.LeftIndent = 0
    $selection.ParagraphFormat.SpaceAfter = 8
    $selection.TypeParagraph()

    Write-Host '[word 5/14] Traceability graph...'
    Add-Heading $selection '3. Traceability-Ãœbersicht' 1
    Add-ChapterDescription $selection $ChapterDescriptions.Matrix
    $graphPath = Join-Path $localWork 'traceability-graph.svg'
    $graphSvg = New-TraceabilitySvg $items $itemsByUid $graphPath
    try {
        $graphShape = $selection.InlineShapes.AddPicture($graphPath,$false,$true)
        $graphShape.LockAspectRatio = -1
        $graphShape.Width = $word.CentimetersToPoints(16.0)
        $selection.TypeParagraph()
    }
    catch {
        Write-Warning "Traceability graph could not be inserted: $($_.Exception.Message)"
        Add-BodyParagraph $selection 'Der grafische Nachweis konnte nicht eingefÃ¼gt werden. Der textbasierte Traceability-Baum bleibt vollstÃ¤ndig erhalten.'
    }
    Write-Host '[word 6/14] Classification and risks...'
    Add-Heading $selection '4. Klassifizierung und Risiken' 1
    Add-ChapterDescription $selection $ChapterDescriptions.Risk
    foreach ($item in @($items | Where-Object { $_.type -in @('CLASS','RISK') })) {
        Add-Heading $selection "$($item.uid) - $($item.title)" 2
        Add-KeyValuePairs $selection @(
            @('Anforderungsart', $(Get-TypeName $item.type)),
            @('Status', $item.status),
            @('Beschreibung', $item.text),
            @('Begründung', $item.rationale)
        )
        if ($item.attributes.Count) {
            Add-Heading $selection 'Fachliche Angaben' 4
            $rows = @()
            foreach ($key in $item.attributes.Keys) { $rows += ,@((Get-FieldLabel $key), (Join-DisplayValue $item.attributes[$key])) }
            Add-KeyValuePairs $selection $rows
        }
    }

    Write-Host '[word 7/14] Requirement details...'
    Add-Heading $selection '5. Anforderungen im Detail' 1
    Add-ChapterDescription $selection $ChapterDescriptions.Requirements
    foreach ($item in @($items | Where-Object { $_.type -notin @('CLASS','RISK','UT','IT','ST','AT','TEST') })) {
        Add-Heading $selection "$($item.uid) - $($item.title)" 2
        Add-KeyValuePairs $selection @(
            @('Anforderungsart', $(Get-TypeName $item.type)),
            @('Status', $item.status),
            @('Version', $item.version),
            @('Anforderung', $item.text),
            @('Begründung', $item.rationale),
            @('Parents', $(Join-DisplayValue $item.parents)),
            @('Children', $(Join-DisplayValue $item.children))
        )
        if ($item.attributes.Count) {
            Add-Heading $selection 'Typspezifische Angaben' 4
            $rows = @()
            foreach ($key in $item.attributes.Keys) { $rows += ,@((Get-FieldLabel $key), (Join-DisplayValue $item.attributes[$key])) }
            Add-KeyValuePairs $selection $rows
        }
        Add-ImplementationEvidence $selection $item
        Add-ImplementationAndScanEvidence $selection $item
    }

    Write-Host '[word 8/14] Test evidence...'
    Add-Heading $selection '6. Test- und Verifikationsnachweise' 1
    Add-ChapterDescription $selection $ChapterDescriptions.Tests
    foreach ($item in @($items | Where-Object { $_.type -in @('UT','IT','ST','AT','TEST') })) {
        Add-Heading $selection "$($item.uid) - $($item.title)" 2
        Add-KeyValuePairs $selection @(
            @('Testart', $(Get-TypeName $item.type)),
            @('Status', $item.status),
            @('Testbeschreibung', $item.text),
            @('Begründung', $item.rationale),
            @('Verifizierte Anforderungen', $(Join-DisplayValue $item.attributes.verifies)),
            @('Testmethode', $(Join-DisplayValue $item.attributes.test_method)),
            @('Erwartetes Ergebnis', $(Join-DisplayValue $item.attributes.expected_result)),
            @('Verantwortlich', $(Join-DisplayValue $item.attributes.test_responsible)),
            @('Parents', $(Join-DisplayValue $item.parents))
        )
    }

    Write-Host '[word 9/14] Audit evidence...'
    Add-Heading $selection '7. Audit- und Berichtsnachweise' 1
    Add-ChapterDescription $selection $ChapterDescriptions.Audit
    $historyPath = Join-Path $reqs 'captainslog.yml'
    $historyEntries = if (Test-Path $historyPath) { ([regex]::Matches([IO.File]::ReadAllText($historyPath), '(?m)^\s*- uid:')).Count } else { 0 }
    Add-KeyValuePairs $selection @(
        @('Historien-Datei', $historyPath),
        @('Audit-Einträge', [string]$historyEntries),
        @('Signaturprüfung', $(if ($verificationExitCode -eq 0) { 'Alle Signaturen gültig' } else { 'Fehlgeschlagen' })),
        @('Berichtsquelle', 'Strukturierter Word-Bericht aus lokalen Requirement-YAML-Dateien'),
        @('Projektpfad', $Project),
        @('Git-Revision', $gitRevision),
        @('Erstellungsdatum', $generatedDateTime)
    )

    if ($document.TablesOfContents.Count -gt 0) { $document.TablesOfContents.Item(1).Update() }
    Write-Host '[word 10/14] Saving DOCX to local temporary storage...'
    $saveArguments = [object[]]@([string]$docx, [int]16)
    $document.GetType().InvokeMember(
        'SaveAs2',
        [System.Reflection.BindingFlags]::InvokeMethod,
        $null,
        $document,
        $saveArguments,
        [System.Globalization.CultureInfo]::InvariantCulture
    ) | Out-Null
    Write-Host '[word 11/14] Local DOCX saved. PDF export deferred.'
}
finally {
    if ($document) { $document.Close(0) }
    if ($word) { $word.Quit() }
    [GC]::Collect(); [GC]::WaitForPendingFinalizers()
}

if (-not (Test-Path -LiteralPath $docx)) { throw "Local DOCX was not created: $docx" }

Write-Host '[word 12/14] Starting fresh Word process for PDF export...'
$pdfWord = $null; $pdfDocument = $null
try {
    $pdfWord = New-Object -ComObject Word.Application
    $pdfWord.Visible = $false
    $pdfWord.DisplayAlerts = 0
    $pdfWord.ScreenUpdating = $false
    $pdfDocument = $pdfWord.Documents.Open([string]$docx, $false, $true, $false)
    Write-Host '[word 13/14] Exporting reopened DOCX to PDF...'
    $pdfDocument.ExportAsFixedFormat([string]$pdf, 17, $false, 0, 0, 1, 9999, 0, $true, $true, 1, $true, $true, $false)
    Write-Host '[word 14/14] Separate PDF export completed.'
}
finally {
    if ($pdfDocument) { $pdfDocument.Close(0) }
    if ($pdfWord) { $pdfWord.Quit() }
    [GC]::Collect(); [GC]::WaitForPendingFinalizers()
}

if (-not (Test-Path -LiteralPath $pdf)) { throw "Local PDF was not created: $pdf" }
Write-Host '[word post] Moving completed files to reports...'
Copy-Item -LiteralPath $docx -Destination $finalDocx -Force
Copy-Item -LiteralPath $pdf -Destination $finalPdf -Force
Remove-Item -LiteralPath $localWork -Recurse -Force -ErrorAction SilentlyContinue

$manifest = [ordered]@{
    schema_version = 4
    project = $projectName
    generated_at = [DateTime]::UtcNow.ToString('O')
    git_revision = $gitRevision
    draft = [bool]$Draft
    validation_exit_code = $validationExitCode
    verification_exit_code = $verificationExitCode
    objects = $items.Count
    docx = $finalDocx
    pdf = $finalPdf
    docx_sha256 = (Get-FileHash -LiteralPath $finalDocx -Algorithm SHA256).Hash
    pdf_sha256 = (Get-FileHash -LiteralPath $finalPdf -Algorithm SHA256).Hash
    logo_sha256 = (Get-FileHash -LiteralPath $logo -Algorithm SHA256).Hash
}
$manifestPath = Join-Path $OutputDirectory "captains-log-traceability-$stamp.manifest.json"
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

Write-Host "Word-style Captain's Log report created."
Write-Host "PDF: $finalPdf"
Write-Host "Manifest: $manifestPath"
Write-Host "DOCX: $finalDocx"
