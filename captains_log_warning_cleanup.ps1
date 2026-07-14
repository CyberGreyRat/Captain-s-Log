$ErrorActionPreference = "Stop"
$CliRoot = "C:\Users\luec-a\Documents\CapCom\cli"

function Replace-Required {
    param(
        [string]$Path,
        [string]$Old,
        [string]$New
    )

    $text = [System.IO.File]::ReadAllText($Path)
    if (-not $text.Contains($Old)) {
        throw "Expected source block not found in: $Path"
    }
    $text = $text.Replace($Old, $New)
    [System.IO.File]::WriteAllText(
        $Path,
        $text,
        [System.Text.UTF8Encoding]::new($false)
    )
    Write-Host "Bereinigt: $Path"
}

function Remove-RequiredRegex {
    param(
        [string]$Path,
        [string]$Pattern
    )

    $text = [System.IO.File]::ReadAllText($Path)
    $updated = [regex]::Replace(
        $text,
        $Pattern,
        "",
        [System.Text.RegularExpressions.RegexOptions]::Singleline
    )
    if ($updated -eq $text) {
        throw "Obsolete helper block not found in: $Path"
    }
    [System.IO.File]::WriteAllText(
        $Path,
        $updated,
        [System.Text.UTF8Encoding]::new($false)
    )
    Write-Host "Alte Hilfsfunktionen entfernt: $Path"
}

if (-not (Test-Path $CliRoot)) {
    throw "CLI-Verzeichnis nicht gefunden: $CliRoot"
}

$create = Join-Path $CliRoot "src\commands\create_command.cpp"
Replace-Required $create `
' capcom::config::ConfigLoader{}.load(project);std::transform(type.begin(),type.end(),type.begin(),[](unsigned char c){return static_cast<char>(std::toupper(c));});' `
@'
 const auto config = capcom::config::ConfigLoader{}.load(project);
 (void)config;
 std::transform(
     type.begin(),
     type.end(),
     type.begin(),
     [](const unsigned char character) {
         return static_cast<char>(std::toupper(character));
     });
'@

Replace-Required $create `
' if(!allowed(type))throw std::runtime_error("Unsupported type: "+type);if(title.empty()||title.size()>300)throw std::runtime_error("Title must contain 1-300 characters.");' `
@'
 if (!allowed(type)) {
     throw std::runtime_error("Unsupported type: " + type);
 }
 if (title.empty() || title.size() > 300) {
     throw std::runtime_error("Title must contain 1-300 characters.");
 }
'@

$publish = Join-Path $CliRoot "src\commands\publish_command.cpp"
Remove-RequiredRegex $publish 'std::string unquote\(std::string value\) \{.*?\}\s*(?=std::string html_escape)'
Remove-RequiredRegex $publish 'std::vector<std::string> read_lines\(const std::filesystem::path& file\) \{.*?\}\s*(?=std::vector<AuditEntry> read_history)'

$review = Join-Path $CliRoot "src\commands\review_command.cpp"
Remove-RequiredRegex $review 'std::string first_author_key\(.*?(?=bool technical_type)'

$yaml = Join-Path $CliRoot "src\commands\yaml_commands.cpp"
Replace-Required $yaml `
' if(results.empty())throw std::runtime_error("No Captain''s Log test IDs found.");auto identity=capcom::identity::IdentityManager{}.load_or_create();capcom::audit::HistoryService h{identity};for(auto&[uid,r]:results){auto current=store.load_all();auto found=current.find(uid);if(found==current.end())throw std::runtime_error("Test UID not found: "+uid);auto type=found->second.type;if(!(type=="UT"||type=="IT"||type=="ST"||type=="AT"||type=="TEST"))throw std::runtime_error("Test result must target UT, IT, ST, AT or TEST: "+uid);store.update_test(uid,r);auto item=store.load_all().at(uid);h.append(item.file,uid,r.status=="Passed"?"TEST_PASSED":"TEST_FAILED","Imported from "+src);}std::cout<<"Imported "<<results.size()<<" test result(s).\n";return 0;}' `
@'
 if (results.empty()) {
     throw std::runtime_error("No Captain's Log test IDs found.");
 }

 const auto identity =
     capcom::identity::IdentityManager{}.load_or_create();
 capcom::audit::HistoryService history{identity};

 for (auto& [uid, result] : results) {
     const auto current = store.load_all();
     const auto found = current.find(uid);
     if (found == current.end()) {
         throw std::runtime_error("Test UID not found: " + uid);
     }

     const auto& type = found->second.type;
     if (!(type == "UT" || type == "IT" || type == "ST" ||
           type == "AT" || type == "TEST")) {
         throw std::runtime_error(
             "Test result must target UT, IT, ST, AT or TEST: " + uid);
     }

     store.update_test(uid, result);
     const auto item = store.load_all().at(uid);
     history.append(
         item.file,
         uid,
         result.status == "Passed" ? "TEST_PASSED" : "TEST_FAILED",
         "Imported from " + src);
 }

 std::cout << "Imported " << results.size()
           << " test result(s).\n";
 return 0;
}
'@

Set-Location $CliRoot
$log = Join-Path $CliRoot "warning-clean-build.txt"
cmake --build --preset default --clean-first 2>&1 |
    Tee-Object -FilePath $log

$warnings = Select-String -Path $log -Pattern "warning:"
if ($warnings) {
    $warnings | ForEach-Object { Write-Host $_.Line }
    throw "Build enthaelt weiterhin Compiler-Warnungen."
}

Write-Host ""
Write-Host "Erfolg: Captain's Log wurde ohne Compiler-Warnungen gebaut."
