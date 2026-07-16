$ErrorActionPreference = "Stop"

$root = "C:\Users\luec-a\Documents\CapCom"
$wordScript = Join-Path $root "Publish-CaptainsLog-WordReport-Final.ps1"
$cpp = Join-Path $root "cli\src\commands\publish_command.cpp"
$cliBuild = Join-Path $root "cli\build\gui"
$ninja = Join-Path $root ".venv\Scripts\ninja.exe"
$capTarget = Join-Path $root ".venv\Scripts\cap.exe"

foreach ($required in @($wordScript, $cpp, $ninja)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Required file not found: $required" }
}

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$backup = Join-Path $root "report-enhancement-backup-$stamp"
New-Item -Path $backup -ItemType Directory -Force | Out-Null
Copy-Item -LiteralPath $wordScript -Destination $backup -Force
Copy-Item -LiteralPath $cpp -Destination $backup -Force
if (Test-Path -LiteralPath $capTarget) { Copy-Item -LiteralPath $capTarget -Destination $backup -Force }

# -----------------------------------------------------------------------------
# WORD / PDF
# -----------------------------------------------------------------------------
$word = [System.IO.File]::ReadAllText($wordScript, [System.Text.Encoding]::UTF8)

function Replace-AllRequired {
    param([string]$Text,[string]$Old,[string]$New,[string]$Name)
    if (-not $Text.Contains($Old)) { throw "Word patch point not found: $Name" }
    return $Text.Replace($Old,$New)
}

# Heading 4 for small technical headings. Heading 4 is outside the TOC range.
$word = $word.Replace(
    '$styleId = switch ($Level) { 1 { -2 } 2 { -3 } 3 { -4 } default { -1 } }',
    '$styleId = switch ($Level) { 1 { -2 } 2 { -3 } 3 { -4 } 4 { -5 } default { -1 } }'
)
$word = $word.Replace('foreach ($styleId in @(-2,-3,-4))','foreach ($styleId in @(-2,-3,-4,-5))')
$word = $word.Replace(
    '$document.Styles.Item(-4).Font.Size = 10',
    '$document.Styles.Item(-4).Font.Size = 10' + "`r`n    " + '$document.Styles.Item(-5).Font.Size = 9'
)
$word = $word.Replace("Add-Heading `$selection 'Fachliche Angaben' 3","Add-Heading `$selection 'Fachliche Angaben' 4")
$word = $word.Replace("Add-Heading `$selection 'Typspezifische Angaben' 3","Add-Heading `$selection 'Typspezifische Angaben' 4")

# Remove any header logo insertion block. Keep the compact text header.
$word = [regex]::Replace(
    $word,
    '(?s)\s*# A logo failure must never prevent report creation\..*?Write-Warning "Header logo was skipped: \$\(\$_.Exception.Message\)"\s*\}\s*',
    "`r`n"
)

# Compact cover logo: height matches a restrained title block, not a large banner.
$word = $word.Replace(
    '$coverLogo.Height = $word.CentimetersToPoints(2.3)',
    '$coverLogo.Height = $word.CentimetersToPoints(1.55)'
)

# Replace manual contents list with a real Word TOC when the known block exists.
$tocStart = $word.IndexOf("    Add-Heading `$selection 'Berichtsinhalt' 2")
if ($tocStart -lt 0) { $tocStart = $word.IndexOf("    Add-Heading `$selection 'Inhalt' 2") }
$summaryMarker = "    Write-Host '[word 3/14] Management summary...'"
$tocEnd = $word.IndexOf($summaryMarker, [Math]::Max(0,$tocStart))
if ($tocStart -ge 0 -and $tocEnd -gt $tocStart) {
    $tocBlock = @'
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

'@
    $word = $word.Substring(0,$tocStart) + $tocBlock + $word.Substring($tocEnd)
}
else { throw "Word TOC replacement area not found." }

# Extend YAML item model and parser for cap scan implementation records.
$word = $word.Replace(
    "version = ''; git_hash = ''; attributes = [ordered]@{}",
    "version = ''; git_hash = ''; attributes = [ordered]@{}; implementations = @()"
)
$word = $word.Replace(
    "    `$section = ''`r`n    foreach (`$raw",
    "    `$section = ''`r`n    `$currentImplementation = `$null`r`n    foreach (`$raw"
)
if (-not $word.Contains('$currentImplementation')) {
    $word = $word.Replace(
        "    `$section = ''`n    foreach (`$raw",
        "    `$section = ''`n    `$currentImplementation = `$null`n    foreach (`$raw"
    )
}

$attributeTail = @'
        if ($section -eq 'attributes' -and $raw -match '^\s{2}([A-Za-z0-9_]+):\s*(.*)$') {
            $key = $matches[1]; $value = $matches[2]
            if ($value.Trim().StartsWith('[')) { $item.attributes[$key] = @(Parse-ListValue $value) }
            else { $item.attributes[$key] = Unquote-Value $value }
        }
'@
$implementationTail = @'
        if ($section -eq 'attributes' -and $raw -match '^\s{2}([A-Za-z0-9_]+):\s*(.*)$') {
            $key = $matches[1]; $value = $matches[2]
            if ($value.Trim().StartsWith('[')) { $item.attributes[$key] = @(Parse-ListValue $value) }
            else { $item.attributes[$key] = Unquote-Value $value }
        }
        elseif ($section -eq 'implementations' -and $raw -match '^\s{2}-\s*([A-Za-z0-9_]+):\s*(.*)$') {
            if ($currentImplementation) { $item.implementations += [pscustomobject]$currentImplementation }
            $currentImplementation = [ordered]@{}
            $currentImplementation[$matches[1]] = Unquote-Value $matches[2]
        }
        elseif ($section -eq 'implementations' -and $currentImplementation -and $raw -match '^\s{4}([A-Za-z0-9_]+):\s*(.*)$') {
            $currentImplementation[$matches[1]] = Unquote-Value $matches[2]
        }
'@
if ($word.Contains($attributeTail) -and -not $word.Contains("elseif (`$section -eq 'implementations'")) {
    $word = $word.Replace($attributeTail,$implementationTail)
    $word = $word.Replace(
        '    return [pscustomobject]$item',
        "    if (`$section -eq 'implementations' -and `$currentImplementation) { `$item.implementations += [pscustomobject]`$currentImplementation }`r`n    return [pscustomobject]`$item"
    )
}

$helperMarker = 'function Set-SummaryCell {'
if (-not $word.Contains('function Add-ImplementationEvidence')) {
    if (-not $word.Contains($helperMarker)) { throw "Word helper insertion point not found." }
    $helpers = @'
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

'@
    $word = $word.Replace($helperMarker,$helpers+$helperMarker)
}

# Replace the textual relationship list in chapter 3 with a generated SVG graph.
$graphStart = $word.IndexOf("    Write-Host '[word 5/14]")
$graphEnd = $word.IndexOf("    Write-Host '[word 6/14] Classification and risks...'",$graphStart)
if($graphStart -ge 0 -and $graphEnd -gt $graphStart){
$graphBlock=@'
    Write-Host '[word 5/14] Traceability graph...'
    Add-Heading $selection '3. Traceability-Übersicht' 1
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
        Add-BodyParagraph $selection 'Der grafische Nachweis konnte nicht eingefügt werden. Der textbasierte Traceability-Baum bleibt vollständig erhalten.'
    }

'@
$word=$word.Substring(0,$graphStart)+$graphBlock+$word.Substring($graphEnd)
}else{throw "Traceability chapter replacement area not found."}

# Add implementation evidence to requirement and test loops.
$reqBoundary = "    Write-Host '[word 8/14] Test evidence...'"
$reqPos = $word.IndexOf($reqBoundary)
if($reqPos -gt 0 -and -not $word.Substring(0,$reqPos).Contains('Add-ImplementationEvidence $selection $item')){
    $before=$word.Substring(0,$reqPos);$last=$before.LastIndexOf("    }`r`n`r`n");if($last-lt0){$last=$before.LastIndexOf("    }`n`n")};if($last-lt0){throw'Requirement loop end not found'};$word=$word.Insert($last,"        Add-ImplementationEvidence `$selection `$item`r`n")
}
$testBoundary = "    Write-Host '[word 9/14] Audit evidence...'"
$testPos = $word.IndexOf($testBoundary)
if($testPos -gt 0){$before=$word.Substring(0,$testPos);if(-not $before.Substring([Math]::Max(0,$before.Length-2000)).Contains('Add-ImplementationEvidence $selection $item')){$last=$before.LastIndexOf("    }`r`n`r`n");if($last-lt0){$last=$before.LastIndexOf("    }`n`n")};if($last-lt0){throw'Test loop end not found'};$word=$word.Insert($last,"        Add-ImplementationEvidence `$selection `$item`r`n")}}

# Update TOC immediately before saving.
$word=$word.Replace("    Write-Host '[word 10/14] Saving DOCX", "    if (`$document.TablesOfContents.Count -gt 0) { `$document.TablesOfContents.Item(1).Update() }`r`n    Write-Host '[word 10/14] Saving DOCX")

# Save with BOM and parse before touching C++.
[System.IO.File]::WriteAllText($wordScript,$word,[System.Text.UTF8Encoding]::new($true))
$tokens=$null;$parseErrors=$null
[System.Management.Automation.Language.Parser]::ParseFile($wordScript,[ref]$tokens,[ref]$parseErrors)|Out-Null
if($parseErrors.Count -gt 0){Copy-Item -LiteralPath (Join-Path $backup (Split-Path $wordScript -Leaf)) -Destination $wordScript -Force;$messages=$parseErrors|ForEach-Object{"Line $($_.Extent.StartLineNumber): $($_.Message)"};throw "Word script syntax check failed and backup was restored:`n$($messages -join "`n")"}

# -----------------------------------------------------------------------------
# HTML REPORT: add an interactive SVG graph generated from the existing tree.
# This is self-contained and needs no CDN or external JavaScript library.
# -----------------------------------------------------------------------------
$cppText=[System.IO.File]::ReadAllText($cpp,[System.Text.Encoding]::UTF8)
if(-not $cppText.Contains('id="traceability-graph"')){
    $layoutNeedle='<div class="report-layout grid gap-6 lg:grid-cols-[22rem_minmax(0,1fr)]">'
    if(-not $cppText.Contains($layoutNeedle)){throw "HTML graph insertion point not found in publish_command.cpp"}
    $graphHtml=@'
<section class="mb-8 rounded-lg border border-slate-200 bg-white p-5 shadow-sm">
  <div class="flex items-center justify-between gap-4">
    <div><h2 class="text-lg font-bold text-blue-900">Traceability Graph</h2><p class="mt-1 text-sm text-slate-500">Knoten anklicken, um zur Anforderung zu springen.</p></div>
    <div class="flex gap-2"><button id="graph-minus" class="rounded border px-3 py-1 text-sm">-</button><button id="graph-reset" class="rounded border px-3 py-1 text-sm">100%</button><button id="graph-plus" class="rounded border px-3 py-1 text-sm">+</button></div>
  </div>
  <div class="mt-4 overflow-auto rounded border border-slate-100 bg-slate-50 p-3"><svg id="traceability-graph" class="min-h-64 min-w-full" role="img" aria-label="Traceability graph"></svg></div>
</section>
'@
    $cppText=$cppText.Replace($layoutNeedle,$graphHtml+$layoutNeedle)
    $scriptNeedle='        const searchInput = document.getElementById("requirement-search");'
    if(-not $cppText.Contains($scriptNeedle)){throw "HTML script insertion point not found"}
    $graphJs=@'
        const graphSvg = document.getElementById("traceability-graph");
        const graphNodes = [...document.querySelectorAll(".tree-node summary")].map((summary) => {
            const link = summary.querySelector('a[href^="#"]');
            if (!link) return null;
            const uid = link.textContent.trim();
            const article = document.getElementById(uid);
            const title = article?.querySelector("h2")?.textContent?.trim() || uid;
            const depth = Math.max(0, summary.closest("li")?.parentElement?.closest("li") ? 1 : 0);
            return { uid, title, summary, depth };
        }).filter(Boolean).filter((value, index, all) => all.findIndex(x => x.uid === value.uid) === index);
        const parentMap = new Map();
        document.querySelectorAll('.requirement-card').forEach(card => {
            const uid = card.id;
            const links = [...card.querySelectorAll('a[href^="#"]')].map(a => a.getAttribute('href').slice(1));
            parentMap.set(uid, links);
        });
        function drawGraph(scale = 1) {
            if (!graphSvg) return;
            const width = Math.max(900, graphNodes.length * 145), height = 360;
            graphSvg.setAttribute("viewBox", `0 0 ${width} ${height}`);
            graphSvg.style.width = `${width * scale}px`;
            const positions = new Map();
            graphNodes.forEach((node, index) => positions.set(node.uid, {x: 70 + index * 140, y: 90 + (index % 3) * 90}));
            const esc = value => String(value).replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;');
            let html = '<defs><marker id="ga" markerWidth="8" markerHeight="8" refX="7" refY="3" orient="auto"><path d="M0,0 L0,6 L8,3 z" fill="#98A2B3"/></marker></defs>';
            parentMap.forEach((targets, uid) => targets.forEach(target => { const a=positions.get(uid), b=positions.get(target); if(a&&b) html += `<line x1="${a.x}" y1="${a.y}" x2="${b.x}" y2="${b.y}" stroke="#98A2B3" stroke-width="2" marker-end="url(#ga)"/>`; }));
            graphNodes.forEach(node => { const p=positions.get(node.uid); html += `<a href="#${esc(node.uid)}"><circle cx="${p.x}" cy="${p.y}" r="27" fill="#0b2d52" stroke="#d0a53a" stroke-width="3"/><text x="${p.x}" y="${p.y+4}" text-anchor="middle" font-size="9" font-weight="700" fill="white">${esc(node.uid)}</text><text x="${p.x}" y="${p.y+45}" text-anchor="middle" font-size="10" fill="#334155">${esc(node.title.slice(0,28))}</text></a>`; });
            graphSvg.innerHTML = html;
        }
        let graphScale = 1; drawGraph(graphScale);
        document.getElementById("graph-plus")?.addEventListener("click", () => drawGraph(graphScale = Math.min(2, graphScale + .15)));
        document.getElementById("graph-minus")?.addEventListener("click", () => drawGraph(graphScale = Math.max(.5, graphScale - .15)));
        document.getElementById("graph-reset")?.addEventListener("click", () => drawGraph(graphScale = 1));
'@
    $cppText=$cppText.Replace($scriptNeedle,$graphJs+$scriptNeedle)
    [System.IO.File]::WriteAllText($cpp,$cppText,[System.Text.UTF8Encoding]::new($false))
}

Write-Host "[1/2] Building Captain CLI with HTML graph..."
$oldPath=$env:PATH;$env:PATH="$(Split-Path $ninja);$env:PATH"
try{& cmake --build $cliBuild --clean-first;if($LASTEXITCODE-ne0){throw "CLI build failed. Backup: $backup"}}finally{$env:PATH=$oldPath}
$newCap=Join-Path $cliBuild 'cap.exe';if(-not(Test-Path -LiteralPath $newCap)){throw "New cap.exe not found: $newCap"}
Stop-Process -Name cap -Force -ErrorAction SilentlyContinue
Copy-Item -LiteralPath $newCap -Destination $capTarget -Force

Write-Host "[2/2] Report enhancements installed."
Write-Host "Backup: $backup"
Write-Host "Run cap scan and then cap publish."
