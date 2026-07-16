param(
    [string]$ProjectRoot = "C:\Users\luec-a\Documents\CapCom",
    [string]$OutputFile = "",
    [switch]$IncludeGeneratedFiles
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $ProjectRoot -PathType Container)) {
    throw "Project directory not found: $ProjectRoot"
}

$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

if ([string]::IsNullOrWhiteSpace($OutputFile)) {
    $projectName = Split-Path $ProjectRoot -Leaf
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutputFile = Join-Path (Split-Path $ProjectRoot -Parent) "$projectName-Code-$timestamp.txt"
}
elseif (-not [System.IO.Path]::IsPathRooted($OutputFile)) {
    $OutputFile = Join-Path (Get-Location).Path $OutputFile
}

$allowedExtensions = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase
)
@(
    ".c", ".cc", ".cpp", ".cxx",
    ".h", ".hh", ".hpp", ".hxx",
    ".cs", ".java", ".kt", ".kts",
    ".py", ".pyi", ".ps1", ".psm1", ".psd1",
    ".js", ".jsx", ".ts", ".tsx", ".mjs", ".cjs",
    ".html", ".htm", ".css", ".scss", ".sass", ".less",
    ".sql", ".xml", ".xsd", ".xslt",
    ".json", ".jsonc", ".yaml", ".yml", ".toml",
    ".md", ".rst", ".txt",
    ".cmake", ".in", ".ini", ".cfg", ".conf",
    ".sh", ".bash", ".zsh", ".bat", ".cmd",
    ".proto", ".graphql", ".gql", ".vue", ".svelte"
) | ForEach-Object { [void]$allowedExtensions.Add($_) }

$allowedNames = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase
)
@(
    "CMakeLists.txt", "Dockerfile", "Makefile", "GNUmakefile",
    ".gitignore", ".gitattributes", ".editorconfig",
    "requirements.txt", "pyproject.toml", "package.json",
    "package-lock.json", "tsconfig.json"
) | ForEach-Object { [void]$allowedNames.Add($_) }

$excludedDirectoryNames = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase
)
@(
    ".git", ".svn", ".hg", ".idea", ".vs", ".vscode",
    ".venv", "venv", "node_modules", "__pycache__",
    "build", "dist", "out", "bin", "obj", "target",
    "coverage", ".coverage", ".pytest_cache", ".mypy_cache",
    ".ruff_cache", "reports", "backup", "backups"
) | ForEach-Object { [void]$excludedDirectoryNames.Add($_) }

$excludedFilePatterns = @(
    "*.exe", "*.dll", "*.lib", "*.a", "*.so", "*.dylib",
    "*.obj", "*.o", "*.pdb", "*.ilk", "*.class", "*.jar",
    "*.zip", "*.7z", "*.rar", "*.gz", "*.tar",
    "*.png", "*.jpg", "*.jpeg", "*.gif", "*.webp", "*.ico", "*.svg",
    "*.pdf", "*.doc", "*.docx", "*.xls", "*.xlsx", "*.ppt", "*.pptx",
    "*.db", "*.sqlite", "*.sqlite3", "*.pem", "*.key", "*.pfx", "*.p12",
    "*.log", "*.tmp", "*.bak", "*.cache", "*.pyc",
    "*-Code-*.txt", "CaptainsLog-Publish-*.txt"
)

function Test-IsExcludedDirectory {
    param([System.IO.DirectoryInfo]$Directory)

    if ($IncludeGeneratedFiles) {
        return $Directory.Name -in @(".git", ".venv", "node_modules")
    }

    return $excludedDirectoryNames.Contains($Directory.Name) -or
        $Directory.Name -like "professional-publish-backup-*" -or
        $Directory.Name -like "backup-*"
}

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
function Get-ProjectFiles {
    param([string]$Root)

    $pending = [System.Collections.Generic.Stack[System.IO.DirectoryInfo]]::new()
    $pending.Push((Get-Item -LiteralPath $Root))

    while ($pending.Count -gt 0) {
        $directory = $pending.Pop()

        foreach ($childDirectory in Get-ChildItem -LiteralPath $directory.FullName -Directory -Force -ErrorAction SilentlyContinue) {
            if (-not (Test-IsExcludedDirectory $childDirectory)) {
                $pending.Push($childDirectory)
            }
        }

        foreach ($file in Get-ChildItem -LiteralPath $directory.FullName -File -Force -ErrorAction SilentlyContinue) {
            $excluded = $false
            foreach ($pattern in $excludedFilePatterns) {
                if ($file.Name -like $pattern) {
                    $excluded = $true
                    break
                }
            }
            if ($excluded) { continue }

            if ($allowedNames.Contains($file.Name) -or $allowedExtensions.Contains($file.Extension)) {
                $file
            }
        }
    }
}

$files = @(Get-ProjectFiles -Root $ProjectRoot | Sort-Object FullName)
if ($files.Count -eq 0) {
    throw "No source files found below: $ProjectRoot"
}

$outputDirectory = Split-Path $OutputFile -Parent
if (-not (Test-Path -LiteralPath $outputDirectory)) {
    New-Item -Path $outputDirectory -ItemType Directory -Force | Out-Null
}

$utf8 = [System.Text.UTF8Encoding]::new($true)
$writer = [System.IO.StreamWriter]::new($OutputFile, $false, $utf8)

try {
    $writer.WriteLine("Captain's Log project source export")
    $writer.WriteLine("Generated: $([DateTime]::UtcNow.ToString('O'))")
    $writer.WriteLine("Project root: $ProjectRoot")
    $writer.WriteLine("Files: $($files.Count)")
    $writer.WriteLine("")
    $writer.WriteLine("===== FILE INDEX =====")

    foreach ($file in $files) {
        $relativePath = Get-RelativeProjectPath -BasePath $ProjectRoot -FullPath $file.FullName
        $writer.WriteLine($relativePath)
    }

    $writer.WriteLine("===== END FILE INDEX =====")

    foreach ($file in $files) {
        $relativePath = Get-RelativeProjectPath -BasePath $ProjectRoot -FullPath $file.FullName
        $writer.WriteLine("")
        $writer.WriteLine("================================================================================")
        $writer.WriteLine("FILE: $relativePath")
        $writer.WriteLine("SIZE: $($file.Length) bytes")
        $writer.WriteLine("SHA256: $((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash)")
        $writer.WriteLine("================================================================================")

        try {
            $content = [System.IO.File]::ReadAllText($file.FullName, [System.Text.Encoding]::UTF8)
            $writer.Write($content)
            if (-not $content.EndsWith("`n")) {
                $writer.WriteLine()
            }
        }
        catch {
            $writer.WriteLine("[READ ERROR] $($_.Exception.Message)")
        }

        $writer.WriteLine("===== END FILE: $relativePath =====")
    }
}
finally {
    $writer.Dispose()
}

$result = Get-Item -LiteralPath $OutputFile
Write-Host "Project source export completed."
Write-Host "Files:  $($files.Count)"
Write-Host "Output: $($result.FullName)"
Write-Host "Size:   $([Math]::Round($result.Length / 1MB, 2)) MB"
