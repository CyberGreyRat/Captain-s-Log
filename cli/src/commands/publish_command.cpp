#include "capcom/commands/publish_command.hpp"

#include "capcom/audit/history_service.hpp"
#include "capcom/identity/identity_manager.hpp"
#include "capcom/yaml/yaml_store.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace capcom::commands {
namespace {

struct AuditEntry {
    std::string timestamp;
    std::string action;
    std::string actor;
    std::string reason;
    std::string key_id;
};

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string unquote(std::string value) {
    value = trim(std::move(value));
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}

std::string html_escape(const std::string& value) {
    std::string output;
    output.reserve(value.size());
    for (const char character : value) {
        switch (character) {
        case '&': output += "&amp;"; break;
        case '<': output += "&lt;"; break;
        case '>': output += "&gt;"; break;
        case '"': output += "&quot;"; break;
        case '\'': output += "&#39;"; break;
        default: output.push_back(character); break;
        }
    }
    return output;
}

std::vector<std::string> read_lines(const std::filesystem::path& file) {
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot read " + file.string());
    std::vector<std::string> result;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        result.push_back(line);
    }
    return result;
}

std::vector<AuditEntry> read_history(const std::filesystem::path& file) {
    const auto lines = read_lines(file);
    std::vector<AuditEntry> entries;
    AuditEntry current{};
    bool in_history = false;

    auto finish = [&]() {
        if (!current.timestamp.empty()) entries.push_back(current);
        current = {};
    };

    for (const auto& line : lines) {
        if (line.rfind("history:", 0) == 0) {
            in_history = true;
            continue;
        }
        if (!in_history) continue;
        if (!line.empty() && line.front() != ' ' && line.front() != '\t') break;

        auto normalized = trim(line);
        if (normalized.rfind("- ", 0) == 0) normalized = trim(normalized.substr(2));
        const auto colon = normalized.find(':');
        if (colon == std::string::npos) continue;
        const auto key = trim(normalized.substr(0, colon));
        const auto value = unquote(normalized.substr(colon + 1));

        if (key == "timestamp") {
            finish();
            current.timestamp = value;
        } else if (key == "action") current.action = value;
        else if (key == "actor") current.actor = value;
        else if (key == "reason") current.reason = value;
        else if (key == "key_id") current.key_id = value;
    }
    finish();
    return entries;
}

std::string generated_at_utc() {
    const auto time = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}

std::string badge_class(const std::string& status) {
    if (status == "Approved" || status == "Tested" || status == "Passed") {
        return "badge badge-green";
    }
    if (status == "Rejected" || status == "Failed") {
        return "badge badge-red";
    }
    return "badge badge-yellow";
}

std::string tree_node(
    const std::string& uid,
    const std::map<std::string, capcom::yaml::Item>& items,
    std::set<std::string> path) {
    const auto iterator = items.find(uid);
    if (iterator == items.end()) {
        return "<li class=\"missing\">" + html_escape(uid) + " [missing]</li>";
    }
    if (!path.insert(uid).second) {
        return "<li><a href=\"#" + html_escape(uid) + "\">" +
            html_escape(uid) + " [reference]</a></li>";
    }

    const auto& item = iterator->second;
    std::ostringstream output;
    output << "<li><details open><summary><a href=\"#" << html_escape(uid)
           << "\"><span class=\"uid\">" << html_escape(uid)
           << "</span> " << html_escape(item.title) << "</a></summary>";

    auto children = item.children;
    std::sort(children.begin(), children.end());
    if (!children.empty()) {
        output << "<ul>";
        for (const auto& child : children) output << tree_node(child, items, path);
        output << "</ul>";
    }
    output << "</details></li>";
    return output.str();
}

std::string render_tree(const std::map<std::string, capcom::yaml::Item>& items) {
    std::vector<std::string> roots;
    for (const auto& [uid, item] : items) {
        if (item.parents.empty()) roots.push_back(uid);
    }
    std::ostringstream output;
    output << "<ul class=\"tree\">";
    for (const auto& root : roots) output << tree_node(root, items, {});
    output << "</ul>";
    return output.str();
}

std::string render_card(const capcom::yaml::Item& item) {
    const auto history = read_history(item.file);
    std::ostringstream output;
    output << "<article id=\"" << html_escape(item.uid)
           << "\" class=\"card requirement\" data-search=\""
           << html_escape(item.uid + " " + item.title + " " + item.status)
           << "\">";
    output << "<div class=\"card-head\"><div><div class=\"uid\">"
           << html_escape(item.uid) << "</div><h2>" << html_escape(item.title)
           << "</h2></div><span class=\"" << badge_class(item.status) << "\">"
           << html_escape(item.status) << "</span></div>";

    output << "<section><h3>Beschreibung</h3><p>"
           << (item.text.empty() ? "<em>Nicht ausgefüllt</em>" : html_escape(item.text))
           << "</p></section>";

    output << "<section><h3>Traceability</h3><p><strong>Parents:</strong> ";
    if (item.parents.empty()) output << "–";
    for (std::size_t index = 0; index < item.parents.size(); ++index) {
        if (index != 0) output << ", ";
        output << "<a href=\"#" << html_escape(item.parents[index]) << "\">"
               << html_escape(item.parents[index]) << "</a>";
    }
    output << "</p><p><strong>Children:</strong> ";
    if (item.children.empty()) output << "–";
    for (std::size_t index = 0; index < item.children.size(); ++index) {
        if (index != 0) output << ", ";
        output << "<a href=\"#" << html_escape(item.children[index]) << "\">"
               << html_escape(item.children[index]) << "</a>";
    }
    output << "</p></section>";

    output << "<section><h3>Code-Verknüpfungen</h3>";
    if (item.implementations.empty()) {
        output << "<p><em>Keine Implementierung gefunden</em></p>";
    } else {
        output << "<div class=\"table-wrap\"><table><thead><tr><th>Datei</th>"
                  "<th>Zeilen</th><th>Git</th></tr></thead><tbody>";
        for (const auto& implementation : item.implementations) {
            output << "<tr><td><code>" << html_escape(implementation.file)
                   << "</code></td><td>" << implementation.start_line << "–"
                   << implementation.end_line << "</td><td><code>"
                   << html_escape(implementation.git_hash) << "</code></td></tr>";
        }
        output << "</tbody></table></div>";
    }
    output << "</section>";

    output << "<section><h3>Testergebnis</h3>";
    if (item.test.status.empty()) {
        output << "<p><em>Kein Testergebnis importiert</em></p>";
    } else {
        output << "<p><span class=\"" << badge_class(item.test.status) << "\">"
               << html_escape(item.test.status) << "</span></p>"
               << "<p><strong>Quelle:</strong> " << html_escape(item.test.source)
               << "</p><p><strong>Zeit:</strong> " << html_escape(item.test.timestamp)
               << "</p><p>" << html_escape(item.test.message) << "</p>";
    }
    output << "</section>";

    output << "<section><h3>Audit-Historie</h3>"
              "<div class=\"table-wrap\"><table><thead><tr><th>Zeitpunkt</th>"
              "<th>Aktion</th><th>Benutzer</th><th>Grund</th><th>Integrität</th>"
              "</tr></thead><tbody>";
    for (const auto& entry : history) {
        output << "<tr><td>" << html_escape(entry.timestamp) << "</td><td>"
               << html_escape(entry.action) << "</td><td>"
               << html_escape(entry.actor) << "</td><td>"
               << html_escape(entry.reason) << "</td>"
               << "<td><span class=\"verified\">&#128274; Verifiziert</span></td></tr>";
    }
    output << "</tbody></table></div></section></article>";
    return output.str();
}

std::string render_html(const std::map<std::string, capcom::yaml::Item>& items) {
    std::ostringstream cards;
    for (const auto& [uid, item] : items) {
        (void)uid;
        cards << render_card(item);
    }

    std::ostringstream html;
    html << R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Captain's Log Traceability Report</title>
<script src="https://cdn.tailwindcss.com"></script>
<style>
:root{color-scheme:dark;--bg:#020617;--panel:#0f172a;--line:#334155;--text:#e2e8f0;--muted:#94a3b8;--cyan:#67e8f9}
*{box-sizing:border-box}html{scroll-behavior:smooth}body{margin:0;background:var(--bg);color:var(--text);font:14px system-ui,Segoe UI,sans-serif}
a{color:var(--cyan);text-decoration:none}a:hover{text-decoration:underline}.header{position:sticky;top:0;z-index:20;padding:18px 24px;background:#0f172af2;border-bottom:1px solid var(--line);backdrop-filter:blur(10px)}
.header h1{margin:0;font-size:24px}.header p{margin:4px 0 0;color:var(--muted)}.layout{display:grid;grid-template-columns:minmax(280px,360px) 1fr;gap:20px;padding:20px}.sidebar{position:sticky;top:100px;align-self:start;max-height:calc(100vh - 120px);overflow:auto;background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:16px}.sidebar input{width:100%;padding:10px;border-radius:8px;border:1px solid var(--line);background:#020617;color:var(--text);margin-bottom:14px}.tree,.tree ul{list-style:none;padding-left:16px}.tree>li{padding-left:0}.tree li{margin:6px 0}.tree summary{cursor:pointer}.uid{font-family:Consolas,monospace;color:var(--cyan);font-weight:700}.content{min-width:0}.card{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:20px;margin-bottom:20px;scroll-margin-top:100px}.card-head{display:flex;justify-content:space-between;gap:16px}.card h2{margin:4px 0 0;font-size:21px}.card h3{font-size:15px;margin:22px 0 8px;color:#cbd5e1}.card p{line-height:1.55;margin:6px 0}.badge{display:inline-block;height:max-content;padding:4px 10px;border-radius:999px;font-weight:700}.badge-green,.verified{color:#86efac}.badge-green{background:#14532d}.badge-yellow{background:#713f12;color:#fde68a}.badge-red{background:#7f1d1d;color:#fecaca}.table-wrap{overflow:auto}table{width:100%;border-collapse:collapse}th,td{text-align:left;padding:9px;border-bottom:1px solid #1e293b;vertical-align:top}th{color:#cbd5e1}code{font-family:Consolas,monospace;color:#bae6fd}.hidden{display:none!important}
@media(max-width:900px){.layout{grid-template-columns:1fr}.sidebar{position:static;max-height:none}}
@media print{.header,.sidebar{position:static}.layout{display:block}.sidebar input{display:none}.card{break-inside:avoid;background:white;color:black}.header,body{background:white;color:black}}
</style></head><body>
<header class="header"><h1>Captain's Log Traceability Report</h1><p>Erzeugt: )HTML"
         << html_escape(generated_at_utc()) << " · Anforderungen: " << items.size()
         << R"HTML( · Audit-Signaturen: verifiziert</p></header>
<div class="layout"><aside class="sidebar"><input id="search" type="search" placeholder="UID, Titel oder Status suchen..."><h2>Anforderungsbaum</h2>)HTML"
         << render_tree(items)
         << R"HTML(</aside><main class="content">)HTML"
         << cards.str()
         << R"HTML(</main></div>
<script>
const input=document.getElementById('search');
input.addEventListener('input',()=>{const q=input.value.toLocaleLowerCase('de');document.querySelectorAll('.requirement').forEach(card=>{card.classList.toggle('hidden',!card.dataset.search.toLocaleLowerCase('de').includes(q));});});
</script></body></html>)HTML";
    return html.str();
}

void write_atomic(const std::filesystem::path& file, const std::string& content) {
    auto temporary = file;
    temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("Cannot write " + temporary.string());
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!output) throw std::runtime_error("Failed writing " + temporary.string());
    }
    std::error_code error;
    std::filesystem::remove(file, error);
    error.clear();
    std::filesystem::rename(temporary, file, error);
    if (error) throw std::runtime_error("Cannot create report: " + error.message());
}

} // namespace

int PublishCommand::execute(const std::filesystem::path& project) const {
    const auto identity = capcom::identity::IdentityManager{}.load_or_create();
    capcom::audit::HistoryService{identity}.verify_project(project);

    const auto items = capcom::yaml::YamlStore{project}.load_all();
    if (items.empty()) throw std::runtime_error("No YAML requirements found in reqs/.");

    const auto report = project / "report.html";
    write_atomic(report, render_html(items));
    std::cout << "Published verified report: " << report.string() << '\n';
    return 0;
}

} // namespace capcom::commands
