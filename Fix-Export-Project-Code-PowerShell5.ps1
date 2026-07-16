$ErrorActionPreference = "Stop"

$path = "C:\Users\luec-a\Documents\CapCom\Export-Project-Code.ps1"
if (-not (Test-Path -LiteralPath $path)) {
    throw "Exporter not found: $path"
}

$backup = "$path.before-powershell5-fix.bak"
Copy-Item -LiteralPath $path -Destination $backup -Force

$text = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)

$functionMarker = "function Get-ProjectFiles {"
if (-not $text.Contains($functionMarker)) {
    throw "Insertion point not found in exporter."
}

$relativeFunction = @'
function Get-RelativeProjectPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BasePath,

        [Parameter(Mandatory = $true)]
        [string]$FullPath
    )

    $normalizedBase = [System.IO.Path]::GetFullPath($BasePath).TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar,
        [System.IO.Path]::AltDirectorySeparatorChar
    )
    $normalizedFull = [System.IO.Path]::GetFullPath($FullPath)

    $baseWithSeparator = $normalizedBase + [System.IO.Path]::DirectorySeparatorChar
    if ($normalizedFull.StartsWith(
        $baseWithSeparator,
        [System.StringComparison]::OrdinalIgnoreCase
    )) {
        return $normalizedFull.Substring($baseWithSeparator.Length)
    }

    # Fallback for unusual paths, compatible with Windows PowerShell 5.1.
    $baseUri = [System.Uri]::new($baseWithSeparator)
    $fullUri = [System.Uri]::new($normalizedFull)
    $relativeUri = $baseUri.MakeRelativeUri($fullUri)
    return [System.Uri]::UnescapeDataString($relativeUri.ToString()).Replace(
        '/',
        [System.IO.Path]::DirectorySeparatorChar
    )
}

'@

if (-not $text.Contains("function Get-RelativeProjectPath")) {
    $text = $text.Replace(
        $functionMarker,
        $relativeFunction + $functionMarker
    )
}

$text = $text.Replace(
    '[System.IO.Path]::GetRelativePath($ProjectRoot, $file.FullName)',
    'Get-RelativeProjectPath -BasePath $ProjectRoot -FullPath $file.FullName'
)

if ($text.Contains('[System.IO.Path]::GetRelativePath(')) {
    throw "Not all unsupported GetRelativePath calls were replaced."
}

[System.IO.File]::WriteAllText(
    $path,
    $text,
    [System.Text.UTF8Encoding]::new($true)
)

$tokens = $null
$errors = $null
[System.Management.Automation.Language.Parser]::ParseFile(
    $path,
    [ref]$tokens,
    [ref]$errors
) | Out-Null

if ($errors.Count -gt 0) {
    Copy-Item -LiteralPath $backup -Destination $path -Force
    $messages = $errors | ForEach-Object {
        "Line $($_.Extent.StartLineNumber): $($_.Message)"
    }
    throw "Fixed exporter has syntax errors; backup restored:`n$($messages -join "`n")"
}

Write-Host "PowerShell 5.1 compatibility fix installed."
Write-Host "Exporter: $path"
Write-Host "Backup:   $backup"
Write-Host "Run .\Export-Project-Code.ps1 again."
