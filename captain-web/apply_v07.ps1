$ErrorActionPreference = "Stop"
$target = "C:\Users\luec-a\Documents\CapCom\captain-web"
function Copy-Safe($source,$destination){
  if ((Resolve-Path $source).Path -ne [IO.Path]::GetFullPath($destination)) { Copy-Item $source $destination -Force }
}
New-Item "$target\include\captain\auth" -ItemType Directory -Force | Out-Null
New-Item "$target\include\captain\project" -ItemType Directory -Force | Out-Null
New-Item "$target\src\auth" -ItemType Directory -Force | Out-Null
New-Item "$target\src\project" -ItemType Directory -Force | Out-Null
Copy-Safe "$PSScriptRoot\include\captain\auth\auth_service.hpp" "$target\include\captain\auth\auth_service.hpp"
Copy-Safe "$PSScriptRoot\include\captain\project\project_catalog.hpp" "$target\include\captain\project\project_catalog.hpp"
Copy-Safe "$PSScriptRoot\src\auth\auth_service.cpp" "$target\src\auth\auth_service.cpp"
Copy-Safe "$PSScriptRoot\src\project\project_catalog.cpp" "$target\src\project\project_catalog.cpp"
Copy-Safe "$PSScriptRoot\tools\cap-gui.ps1" "$target\cap-gui.ps1"
$cmake="$target\CMakeLists.txt";$text=[IO.File]::ReadAllText($cmake)
if($text -notmatch 'src/auth/auth_service.cpp'){$text=$text.Replace('src/api/cli_bridge.cpp',"src/api/cli_bridge.cpp`r`n        src/auth/auth_service.cpp`r`n        src/project/project_catalog.cpp");[IO.File]::WriteAllText($cmake,$text,[Text.UTF8Encoding]::new($false))}
Write-Host "v0.7 security and project catalog components installed."
