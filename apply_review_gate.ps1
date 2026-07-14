$ErrorActionPreference = "Stop"
$path = Join-Path $PSScriptRoot "cli\src\commands\review_command.cpp"
if (-not (Test-Path $path)) {
    $path = "C:\Users\luec-a\Documents\CapCom\cli\src\commands\review_command.cpp"
}
$text = [System.IO.File]::ReadAllText($path)
$needle = @'
    for (const auto& child : item.children) {
        if (!items.contains(child)) errors.push_back("missing child " + child);
    }
    return errors;
'@
$replacement = @'
    for (const auto& child : item.children) {
        if (!items.contains(child)) errors.push_back("missing child " + child);
    }

    if (item.type == "SRS" || item.type == "SEC") {
        bool has_verification_test = false;
        for (const auto& child_uid : item.children) {
            const auto child = items.find(child_uid);
            if (child == items.end()) continue;
            const auto& type = child->second.type;
            if (!(type == "TEST" || type == "UT" || type == "IT" ||
                  type == "ST" || type == "AT")) continue;
            has_verification_test = true;
            if (child->second.test.status != "Passed") {
                errors.push_back("verification test " + child_uid + " is not Passed");
            }
        }
        if (!has_verification_test) {
            errors.push_back("no linked verification test child");
        }
    }
    return errors;
'@
if (-not $text.Contains($needle)) {
    throw "Review readiness block not found; patch was not applied."
}
$text = $text.Replace($needle, $replacement)
[System.IO.File]::WriteAllText($path, $text, [System.Text.UTF8Encoding]::new($false))
Write-Host "Review gate applied: linked tests must be Passed before SRS/SEC approval."
