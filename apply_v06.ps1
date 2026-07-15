$path = ".\captain-web\web\index.html"
$html = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
$html = $html.Replace("Letzte Ã„nderung", "Letzte Änderung")
[System.IO.File]::WriteAllText(
    $path,
    $html,
    [System.Text.UTF8Encoding]::new($false)
)