param(
    [string]$Project = (Get-Location).Path,
    [string]$OutputDirectory = "",
    [switch]$Draft,
    [switch]$KeepDocx
)

$ErrorActionPreference = "Stop"

function Unquote([string]$value) {
    $value = $value.Trim()
    if ($value.Length -ge 2 -and $value[0] -eq '"' -and $value[$value.Length-1] -eq '"') {
        $value = $value.Substring(1,$value.Length-2)
        $value = $value.Replace('\n',"`n").Replace('\r',"`r").Replace('\t',"`t").Replace('\"','"').Replace('\\','\')
    }
    return $value
}

function Parse-List([string]$value) {
    $value = $value.Trim()
    if (-not ($value.StartsWith('[') -and $value.EndsWith(']'))) { return @() }
    $inner = $value.Substring(1,$value.Length-2).Trim()
    if (-not $inner) { return @() }
    return @($inner.Split(',') | ForEach-Object { Unquote $_ } | Where-Object { $_ })
}

function Read-Requirement([string]$path) {
    $item = [ordered]@{
        uid='';type='';status='';title='';text='';rationale='';version='';git_hash=''
        attributes=[ordered]@{};parents=@();children=@();file=$path
    }
    $section = ''
    foreach ($raw in [System.IO.File]::ReadAllLines($path,[System.Text.Encoding]::UTF8)) {
        if (-not $raw.Trim() -or $raw.TrimStart().StartsWith('#')) { continue }
        if ($raw -match '^([A-Za-z0-9_]+):\s*(.*)$') {
            $key=$matches[1];$value=$matches[2];$section=''
            if ($key -eq 'attributes') { $section='attributes';continue }
            if ($key -eq 'integration_template' -or $key -eq 'history' -or $key -eq 'implementations' -or $key -eq 'test') { $section=$key;continue }
            if ($key -eq 'parents' -or $key -eq 'children') { $item[$key]=Parse-List $value }
            elseif ($item.Contains($key)) { $item[$key]=Unquote $value }
            continue
        }
        if ($section -eq 'attributes' -and $raw -match '^\s{2}([A-Za-z0-9_]+):\s*(.*)$') {
            $key=$matches[1];$value=$matches[2]
            $parsed=Parse-List $value
            $item.attributes[$key]=if($value.Trim().StartsWith('[')){@($parsed)}else{Unquote $value}
        }
    }
    return [pscustomobject]$item
}

function Set-CellText($cell,[string]$text,[bool]$bold=$false,[int]$color=0) {
    $cell.Range.Text=$text
    $cell.Range.Font.Name='Aptos'
    $cell.Range.Font.Size=9
    $cell.Range.Font.Bold=[int]$bold
    if ($color -ne 0) { $cell.Range.Font.Color=$color }
    $cell.VerticalAlignment=1
}

function Shade-Cell($cell,[string]$hex) {
    $r=[Convert]::ToInt32($hex.Substring(0,2),16)
    $g=[Convert]::ToInt32($hex.Substring(2,2),16)
    $b=[Convert]::ToInt32($hex.Substring(4,2),16)
    $cell.Shading.BackgroundPatternColor=$r+($g*256)+($b*65536)
}

function Add-Heading($selection,[string]$text,[int]$level) {
    $styleId = switch ($level) { 1 {-2} 2 {-3} 3 {-4} default {-1} }; $selection.Style=$styleId
    $selection.TypeText($text)
    $selection.TypeParagraph()
    $selection.Style=-1
}

function Add-Paragraph($selection,[string]$text,[bool]$bold=$false) {
    $selection.Font.Name='Aptos'
    $selection.Font.Size=10
    $selection.Font.Bold=[int]$bold
    $selection.TypeText($text)
    $selection.TypeParagraph()
    $selection.Font.Bold=0
}

function Add-KeyValueTable($document,$selection,[array]$rows) {
    $table=$document.Tables.Add($selection.Range,$rows.Count,2)
    $table.Borders.Enable=1
    $table.AllowAutoFit=$true
    $table.Columns.Item(1).PreferredWidthType=2
    $table.Columns.Item(1).PreferredWidth=24
    $table.Columns.Item(2).PreferredWidthType=2
    $table.Columns.Item(2).PreferredWidth=76
    for($i=0;$i -lt $rows.Count;$i++) {
        Set-CellText $table.Cell($i+1,1) ([string]$rows[$i][0]) $true 0x6F3E14
        Set-CellText $table.Cell($i+1,2) ([string]$rows[$i][1])
        Shade-Cell $table.Cell($i+1,1) 'EAF1F8'
    }
    $selection.SetRange($table.Range.End,$table.Range.End)
    $selection.TypeParagraph()
    return $table
}

function Join-Values($value) {
    if ($null -eq $value) { return '' }
    if ($value -is [System.Array]) { return ($value -join ', ') }
    return [string]$value
}

$Project=(Resolve-Path -LiteralPath $Project).Path
$reqs=Join-Path $Project 'reqs'
if(-not(Test-Path -LiteralPath $reqs)){throw "No Captain project found: $Project"}
if(-not $OutputDirectory){$OutputDirectory=Join-Path $Project 'reports'}
elseif(-not[IO.Path]::IsPathRooted($OutputDirectory)){$OutputDirectory=Join-Path $Project $OutputDirectory}
New-Item $OutputDirectory -ItemType Directory -Force|Out-Null

$logo='C:\Users\luec-a\Documents\CapCom\captain-web\web\logo.png'
if(-not(Test-Path -LiteralPath $logo)){throw "Logo not found: $logo"}
$cap=(Get-Command cap -ErrorAction Stop).Source

Push-Location $Project
try {
    & $cap validate | Out-Host;$validate=$LASTEXITCODE
    & $cap verify | Out-Host;$verify=$LASTEXITCODE
} finally { Pop-Location }
if(($validate-ne 0 -or $verify-ne 0)-and -not $Draft){throw 'Validation or verification failed. Use -Draft for a marked draft report.'}

$items=@(Get-ChildItem $reqs -Recurse -File -Include *.yaml,*.yml |
    Where-Object {$_.Name -notin @('captainslog.yml','captainslog.yaml') -and $_.FullName -notmatch '\\remote\\'} |
    ForEach-Object {Read-Requirement $_.FullName} |
    Where-Object {$_.uid})
$order=@{CLASS=0;RISK=1;USR=2;SYS=3;SRS=4;SEC=5;ARCH=6;LLD=7;UT=8;IT=9;ST=10;AT=11;TEST=12;BUG=13;CR=14;SOUP=15}
$items=@($items|Sort-Object @{Expression={if($order.ContainsKey($_.type)){$order[$_.type]}else{99}}},uid)
$byUid=@{};foreach($item in $items){$byUid[$item.uid]=$item}

function Tree-Lines([string]$uid,[string]$prefix,[bool]$last,[hashtable]$seen) {
    if($seen.ContainsKey($uid)){return @("$prefix+- $uid [cycle]")}
    $nextSeen=@{}+$seen;$nextSeen[$uid]=$true
    $item=$byUid[$uid];if(-not$item){return @("$prefix+- $uid [missing]")}
    $branch=$(if($last){'`- '}else{'|- '})
    $result=@("$prefix$branch$($item.uid)  $($item.title)  [$($item.status)]")
    $kids=@($item.children|Where-Object{$byUid.ContainsKey($_)})
    $childPrefix=$prefix+$(if($last){'   '}else{'|  '})
    for($i=0;$i-lt$kids.Count;$i++){$result+=Tree-Lines $kids[$i] $childPrefix ($i-eq$kids.Count-1) $nextSeen}
    return $result
}
$roots=@($items|Where-Object{$_.parents.Count-eq 0})
$tree=@();for($i=0;$i-lt$roots.Count;$i++){$tree+=Tree-Lines $roots[$i].uid '' ($i-eq$roots.Count-1) @{}}

$stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
$docx=Join-Path $OutputDirectory "captains-log-traceability-$stamp.docx"
$pdf=Join-Path $OutputDirectory "captains-log-traceability-$stamp.pdf"
$projectName=Split-Path $Project -Leaf
$git='working-tree';try{$g=&git -C $Project rev-parse --short HEAD 2>$null;if($LASTEXITCODE-eq 0-and$g){$git=$g.Trim()}}catch{}

$word=$null;$document=$null
try {
    $word=New-Object -ComObject Word.Application
    $word.Visible=$false;$word.DisplayAlerts=0
    $document=$word.Documents.Add()
    $section=$document.Sections.Item(1)
    $section.PageSetup.TopMargin=$word.CentimetersToPoints(1.8)
    $section.PageSetup.BottomMargin=$word.CentimetersToPoints(1.8)
    $section.PageSetup.LeftMargin=$word.CentimetersToPoints(1.8)
    $section.PageSetup.RightMargin=$word.CentimetersToPoints(1.8)

    $normal=$document.Styles.Item(-1);$normal.Font.Name='Aptos';$normal.Font.Size=10
    foreach($styleId in @(-2,-3,-4)){$st=$document.Styles.Item($styleId);$st.Font.Name='Aptos Display';$st.Font.Color=0x6F3E14}
    $document.Styles.Item(-2).Font.Size=18
    $document.Styles.Item(-3).Font.Size=14
    $document.Styles.Item(-4).Font.Size=11

    $header=$section.Headers.Item(1).Range
    $header.Text="Captain's Log  |  $projectName"
    $header.Font.Name='Aptos';$header.Font.Size=8;$header.Font.Color=0x777777
    $footer=$section.Footers.Item(1).Range
    $footer.Text="Requirements - Risk - Verification    |    Revision $git"
    $footer.Font.Name='Aptos';$footer.Font.Size=8;$footer.Font.Color=0x777777

    $sel=$word.Selection
    $coverTable=$document.Tables.Add($sel.Range,1,2)
    $coverTable.Borders.Enable=0
    $coverTable.Columns.Item(1).Width=$word.CentimetersToPoints(5.0)
    $coverTable.Columns.Item(2).Width=$word.CentimetersToPoints(11.5)
    $coverTable.Cell(1,1).Range.InlineShapes.AddPicture($logo,$false,$true)|Out-Null
    $coverTable.Cell(1,2).Range.Text="REQUIREMENTS · RISK · VERIFICATION`rCaptain's Log`rTraceability Report"
    $coverTable.Cell(1,2).Range.Font.Name='Aptos Display';$coverTable.Cell(1,2).Range.Font.Color=0xFFFFFF
    $coverTable.Cell(1,2).Range.Paragraphs.Item(1).Range.Font.Size=9
    $coverTable.Cell(1,2).Range.Paragraphs.Item(2).Range.Font.Size=28;$coverTable.Cell(1,2).Range.Paragraphs.Item(2).Range.Font.Bold=1
    $coverTable.Cell(1,2).Range.Paragraphs.Item(3).Range.Font.Size=17
    foreach($c in 1..2){Shade-Cell $coverTable.Cell(1,$c) '0D3158'}
    $sel.SetRange($coverTable.Range.End,$coverTable.Range.End);$sel.TypeParagraph();$sel.TypeParagraph()
    Add-KeyValueTable $document $sel @(
        @('Projekt',$projectName),@('Berichtsstatus',$(if($Draft){'DRAFT - nicht freigegeben'}else{'Freigegebener Nachweisbericht'})),
        @('Erstellt',(Get-Date -Format 'dd.MM.yyyy HH:mm')),@('Git-Revision',$git),
        @('Validierung',$(if($validate-eq 0){'Erfolgreich'}else{'Fehlgeschlagen'})),
        @('Audit-Signaturen',$(if($verify-eq 0){'Verifiziert'}else{'Nicht verifiziert'}))
    )|Out-Null
    Add-Heading $sel 'Berichtsinhalt' 2
    foreach($line in @('1. Management Summary','2. Traceability-Baum','3. Traceability-Matrix','4. Klassifizierung und Risiken','5. Anforderungen im Detail','6. Test- und Verifikationsnachweise','7. Audit- und Berichtsnachweise')){Add-Paragraph $sel $line}
    $sel.InsertBreak(7)

    Add-Heading $sel '1. Management Summary' 1
    $counts=$items|Group-Object type|Sort-Object Name
    $summary=$document.Tables.Add($sel.Range,$counts.Count+1,3);$summary.Borders.Enable=1
    foreach($c in 1..3){Shade-Cell $summary.Cell(1,$c) '0D3158';Set-CellText $summary.Cell(1,$c) @('Typ','Anzahl','Status')[($c-1)] $true 0xFFFFFF}
    for($i=0;$i-lt$counts.Count;$i++){Set-CellText $summary.Cell($i+2,1) $counts[$i].Name $true;Set-CellText $summary.Cell($i+2,2) ([string]$counts[$i].Count);Set-CellText $summary.Cell($i+2,3) (($counts[$i].Group.status|Sort-Object -Unique)-join', ')}
    $sel.SetRange($summary.Range.End,$summary.Range.End);$sel.TypeParagraph()
    Add-Paragraph $sel "Gesamtobjekte: $($items.Count) | Validierung: $(if($validate-eq 0){'erfolgreich'}else{'fehlgeschlagen'}) | Audit: $(if($verify-eq 0){'verifiziert'}else{'nicht verifiziert'})"

    Add-Heading $sel '2. Traceability-Baum' 1
    Add-Paragraph $sel 'Die Baumdarstellung zeigt die fachlichen Parent-/Child-Beziehungen von oben nach unten.'
    $treeTable=$document.Tables.Add($sel.Range,$tree.Count,1);$treeTable.Borders.Enable=1
    for($i=0;$i-lt$tree.Count;$i++){Set-CellText $treeTable.Cell($i+1,1) $tree[$i];$treeTable.Cell($i+1,1).Range.Font.Name='Consolas';$treeTable.Cell($i+1,1).Range.Font.Size=8.5;if($i%2-eq 0){Shade-Cell $treeTable.Cell($i+1,1) 'F6F8FB'}}
    $sel.SetRange($treeTable.Range.End,$treeTable.Range.End);$sel.TypeParagraph()

    Add-Heading $sel '3. Traceability-Matrix' 1
    $links=@();foreach($item in $items){if($item.children.Count-eq 0){$links+=,[pscustomobject]@{Parent='';Child=$item.uid;Relation='Root/Leaf'}}else{foreach($child in $item.children){$links+=,[pscustomobject]@{Parent=$item.uid;Child=$child;Relation="$($item.type) -> $($byUid[$child].type)"}}}}
    $matrix=$document.Tables.Add($sel.Range,$links.Count+1,4);$matrix.Borders.Enable=1;$matrix.Rows.Item(1).HeadingFormat=-1
    foreach($c in 1..4){Shade-Cell $matrix.Cell(1,$c) '0D3158';Set-CellText $matrix.Cell(1,$c) @('Parent','Child','Beziehung','Nachweis')[($c-1)] $true 0xFFFFFF}
    for($i=0;$i-lt$links.Count;$i++){$l=$links[$i];Set-CellText $matrix.Cell($i+2,1) $l.Parent;Set-CellText $matrix.Cell($i+2,2) $l.Child;Set-CellText $matrix.Cell($i+2,3) $l.Relation;Set-CellText $matrix.Cell($i+2,4) $(if($byUid[$l.Child]){'Vorhanden'}else{'Fehlt'})}
    $sel.SetRange($matrix.Range.End,$matrix.Range.End);$sel.TypeParagraph()

    Add-Heading $sel '4. Klassifizierung und Risiken' 1
    foreach($item in $items|Where-Object{$_.type-in@('CLASS','RISK')}){
        Add-Heading $sel "$($item.uid) - $($item.title)" 2
        Add-KeyValueTable $document $sel @(@('Typ',$item.type),@('Status',$item.status),@('Anforderung',$item.text),@('Begründung',$item.rationale))|Out-Null
        if($item.attributes.Count){$rows=@();foreach($k in $item.attributes.Keys){$rows+=,@($k,$(Join-Values $item.attributes[$k]))};Add-KeyValueTable $document $sel $rows|Out-Null}
    }

    Add-Heading $sel '5. Anforderungen im Detail' 1
    foreach($item in $items|Where-Object{$_.type-notin@('CLASS','RISK','UT','IT','ST','AT','TEST')}){
        Add-Heading $sel "$($item.uid) - $($item.title)" 2
        Add-KeyValueTable $document $sel @(
            @('Typ / Status',"$($item.type) / $($item.status)"),@('Version',$item.version),@('Anforderung',$item.text),@('Begründung',$item.rationale),
            @('Parents',$(Join-Values $item.parents)),@('Children',$(Join-Values $item.children))
        )|Out-Null
        if($item.attributes.Count){Add-Heading $sel 'Typspezifische Angaben' 3;$rows=@();foreach($k in $item.attributes.Keys){$rows+=,@($k,$(Join-Values $item.attributes[$k]))};Add-KeyValueTable $document $sel $rows|Out-Null}
    }

    Add-Heading $sel '6. Test- und Verifikationsnachweise' 1
    foreach($item in $items|Where-Object{$_.type-in@('UT','IT','ST','AT','TEST')}){
        Add-Heading $sel "$($item.uid) - $($item.title)" 2
        Add-KeyValueTable $document $sel @(
            @('Typ / Status',"$($item.type) / $($item.status)"),@('Testbeschreibung',$item.text),@('Begründung',$item.rationale),
            @('Verifiziert',$(Join-Values $item.attributes.verifies)),@('Testmethode',$(Join-Values $item.attributes.test_method)),
            @('Erwartetes Ergebnis',$(Join-Values $item.attributes.expected_result)),@('Verantwortlich',$(Join-Values $item.attributes.test_responsible)),
            @('Parents',$(Join-Values $item.parents))
        )|Out-Null
    }

    Add-Heading $sel '7. Audit- und Berichtsnachweise' 1
    $history=Join-Path $reqs 'captainslog.yml'
    $entries=if(Test-Path $history){([regex]::Matches([IO.File]::ReadAllText($history),'(?m)^\s*- uid:')).Count}else{0}
    Add-KeyValueTable $document $sel @(
        @('Historien-Datei',$history),@('Audit-Einträge',[string]$entries),@('Signaturprüfung',$(if($verify-eq 0){'Alle Signaturen gültig'}else{'Fehlgeschlagen'})),
        @('PDF-Quelle','Strukturierter Word-Bericht aus lokalen Requirement-YAML-Dateien'),@('Projektpfad',$Project),@('Git-Revision',$git)
    )|Out-Null

    $document.SaveAs2($docx,16)
    $document.ExportAsFixedFormat($pdf,17)
}
finally {
    if($document){$document.Close(0)}
    if($word){$word.Quit()}
    [GC]::Collect();[GC]::WaitForPendingFinalizers()
}
if(-not(Test-Path $pdf)){throw 'PDF was not created.'}
$manifest=[ordered]@{schema_version=2;project=$projectName;generated_at=[DateTime]::UtcNow.ToString('O');git_revision=$git;draft=[bool]$Draft;validation_exit_code=$validate;verification_exit_code=$verify;objects=$items.Count;docx=$docx;pdf=$pdf;pdf_sha256=(Get-FileHash $pdf -Algorithm SHA256).Hash;logo_sha256=(Get-FileHash $logo -Algorithm SHA256).Hash}
$manifestPath=Join-Path $OutputDirectory "captains-log-traceability-$stamp.manifest.json"
$manifest|ConvertTo-Json -Depth 5|Set-Content $manifestPath -Encoding UTF8
if(-not$KeepDocx){Remove-Item $docx -Force}
Write-Host "Word-style Captain's Log report created."
Write-Host "PDF: $pdf"
Write-Host "Manifest: $manifestPath"
if($KeepDocx){Write-Host "DOCX: $docx"}
