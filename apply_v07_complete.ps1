$ErrorActionPreference = "Stop"
$target = "C:\Users\luec-a\Documents\CapCom\captain-web"
Stop-Process -Name captain-web -Force -ErrorAction SilentlyContinue
Copy-Item "$PSScriptRoot\include\captain\api\api_server.hpp" "$target\include\captain\api\api_server.hpp" -Force
Copy-Item "$PSScriptRoot\src\api\api_server.cpp" "$target\src\api\api_server.cpp" -Force
Copy-Item "$PSScriptRoot\src\main.cpp" "$target\src\main.cpp" -Force
Copy-Item "$PSScriptRoot\tools\cap-gui.ps1" "$target\cap-gui.ps1" -Force

$index = "$target\web\index.html"
$html = [IO.File]::ReadAllText($index)
if ($html -notmatch 'id="projectSelect"') {
  $html = $html.Replace('<span id="session" class="text-sm"></span>', '<select id="projectSelect" class="rounded border border-blue-600 bg-blue-800 px-3 py-2 text-sm text-white"></select><span id="session" class="text-sm"></span><span id="viewerHint" class="hidden rounded bg-amber-100 px-3 py-2 text-xs font-semibold text-amber-900">Nur Lesen. Im Projektordner cap gui starten.</span>')
  $extra = @'
<script>
(async()=>{
  const session=await api('/session');
  S.canWrite=Boolean(session.can_write);
  S.activeProject=session.active_project;
  S.viewProject=session.active_project;
  const projects=await api('/projects');
  const selectElement=$('projectSelect');
  selectElement.innerHTML=(projects.projects||[]).map(project=>`<option value="${esc(project.id)}" ${project.id===S.activeProject?'selected':''}>${esc(project.name)}${project.id===S.activeProject?' · lokal':' · Snapshot'}</option>`).join('');
  const applyPermissions=()=>{
    $('session').textContent=session.authenticated?`${session.user}@${session.host} · Developer`:'Viewer';
    $('viewerHint').classList.toggle('hidden',S.canWrite);
    ['new','globalLink','push','pull'].forEach(id=>{const element=$(id);if(element)element.classList.toggle('hidden',!S.canWrite)});
    document.querySelectorAll('.cmd').forEach(element=>{const readOnly=['tree','status'].includes(element.dataset.command);element.classList.toggle('hidden',!S.canWrite&&!readOnly)});
    document.querySelectorAll('#detail button,#detail textarea').forEach(element=>element.classList.toggle('hidden',!S.canWrite));
  };
  const originalSelect=select;
  window.setInterval(applyPermissions,250);
  selectElement.onchange=async()=>{
    S.viewProject=selectElement.value;
    S.selected=null;
    if(S.viewProject===S.activeProject){await load();}
    else {const response=await api(`/projects/${S.viewProject}/requirements`);S.items=response.items||[];draw();$('detail').innerHTML='<div class="rounded border border-blue-200 bg-blue-50 p-4 text-sm text-blue-900">Zentraler Read-only-Snapshot. Zum Bearbeiten dieses Projekt lokal öffnen und dort cap gui ausführen.</div>';}
  };
  applyPermissions();
})();
</script>
'@
  $html = $html.Replace('</body>', $extra + '</body>')
  [IO.File]::WriteAllText($index,$html,[Text.UTF8Encoding]::new($false))
}

# Install a PowerShell cap wrapper. Functions have precedence over cap.exe.
$profilePath = $PROFILE.CurrentUserAllHosts
New-Item (Split-Path $profilePath) -ItemType Directory -Force | Out-Null
if (-not (Test-Path $profilePath)) { New-Item $profilePath -ItemType File -Force | Out-Null }
$profileText = [IO.File]::ReadAllText($profilePath)
if ($profileText -notmatch 'CaptainLogCapWrapper') {
$wrapper = @'

# CaptainLogCapWrapper
function global:cap {
    if ($args.Count -gt 0 -and $args[0] -eq "gui") {
        $remaining = @($args | Select-Object -Skip 1)
        & "C:\Users\luec-a\Documents\CapCom\captain-web\cap-gui.ps1" @remaining
        return
    }
    & "C:\Users\luec-a\Documents\CapCom\.venv\Scripts\cap.exe" @args
}
'@
  Add-Content -Path $profilePath -Value $wrapper
}
Write-Host "Captain Web v0.7 complete installed. Open a new PowerShell after rebuilding."
