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

using Items = std::map<std::string, capcom::yaml::Item>;

bool test_type(const std::string& type) {
    return type == "TEST" || type == "UT" || type == "IT" ||
           type == "ST" || type == "AT";
}

bool base_complete(const capcom::yaml::Item& item) {
    if (item.uid.empty() || item.title.empty() || item.text.empty() ||
        item.rationale.empty()) {
        return false;
    }
    if ((item.type == "SRS" || item.type == "SEC") &&
        item.implementations.empty()) {
        return false;
    }
    if (test_type(item.type) &&
        (item.implementations.empty() || item.test.status != "Passed")) {
        return false;
    }
    return true;
}

bool chain_complete(
    const std::string& uid,
    const Items& items,
    std::set<std::string> path) {
    const auto found = items.find(uid);
    if (found == items.end() || !path.insert(uid).second ||
        !base_complete(found->second)) {
        return false;
    }

    const auto& item = found->second;
    if (item.type == "USR" || item.type == "SYS" ||
        item.type == "SRS" || item.type == "SEC") {
        if (item.children.empty()) {
            return false;
        }
        for (const auto& child : item.children) {
            if (!chain_complete(child, items, path)) {
                return false;
            }
        }
    }
    return true;
}

std::string escape(const std::string& value) {
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

std::string generated_at() {
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

std::string state_badge(const std::string& text, const std::string& css) {
    return "<span class=\"badge " + css + "\">" + escape(text) + "</span>";
}

std::string item_signals(const capcom::yaml::Item& item) {
    std::ostringstream output;
    output << state_badge(item.status, "neutral");
    if (!item.implementations.empty()) {
        output << state_badge("Code verknüpft", "good");
    } else if (item.type == "SRS" || item.type == "SEC" ||
               test_type(item.type)) {
        output << state_badge("Code fehlt", "warn");
    }
    if (test_type(item.type)) {
        output << state_badge(
            item.test.status.empty() ? "Nicht ausgeführt" : item.test.status,
            item.test.status == "Passed" ? "good" : "bad");
    }
    return output.str();
}

std::string tree_node(
    const std::string& uid,
    const Items& items,
    std::set<std::string> path) {
    const auto found = items.find(uid);
    if (found == items.end()) {
        return "<li class=\"bad-text\">" + escape(uid) + " fehlt</li>";
    }
    if (!path.insert(uid).second) {
        return "<li><a href=\"#" + escape(uid) + "\">" +
               escape(uid) + " (Referenz)</a></li>";
    }

    const auto& item = found->second;
    std::ostringstream output;
    output << "<li><details><summary><a href=\"#" << escape(uid) << "\">"
           << "<strong>" << escape(uid) << "</strong> "
           << escape(item.title) << "</a> "
           << (chain_complete(uid, items, {})
                   ? state_badge("Kette gültig", "good")
                   : state_badge("Kette offen", "warn"))
           << "</summary>";
    if (!item.children.empty()) {
        output << "<ul>";
        for (const auto& child : item.children) {
            output << tree_node(child, items, path);
        }
        output << "</ul>";
    }
    output << "</details></li>";
    return output.str();
}

std::string tree(const Items& items) {
    std::ostringstream output;
    output << "<ul class=\"tree\">";
    for (const auto& [uid, item] : items) {
        if (item.parents.empty()) {
            output << tree_node(uid, items, {});
        }
    }
    output << "</ul>";
    return output.str();
}

std::string links(const capcom::yaml::Item& item) {
    std::ostringstream output;
    if (!item.parents.empty()) {
        output << "<div><span class=\"muted\">Parent:</span> ";
        for (const auto& uid : item.parents) {
            output << "<a href=\"#" << escape(uid) << "\">" << escape(uid)
                   << "</a> ";
        }
        output << "</div>";
    }
    if (!item.children.empty()) {
        output << "<div><span class=\"muted\">Children:</span> ";
        for (const auto& uid : item.children) {
            output << "<a href=\"#" << escape(uid) << "\">" << escape(uid)
                   << "</a> ";
        }
        output << "</div>";
    }
    return output.str();
}

std::string card(
    const capcom::yaml::Item& item,
    const Items& items,
    const std::vector<capcom::audit::AuditEntry>& history) {
    const bool valid = chain_complete(item.uid, items, {});
    std::ostringstream output;
    output << "<article id=\"" << escape(item.uid)
           << "\" class=\"card requirement\" data-search=\""
           << escape(item.uid + " " + item.title + " " + item.text) << "\">"
           << "<details class=\"item\"><summary class=\"item-summary\">"
           << "<div><span class=\"uid\">" << escape(item.uid)
           << "</span><h2>" << escape(item.title) << "</h2>"
           << "<p class=\"preview\">" << escape(item.text) << "</p></div>"
           << "<div class=\"signals\">" << item_signals(item)
           << (valid ? state_badge("Kette gültig", "good")
                     : state_badge("Kette unvollständig", "warn"))
           << "</div></summary><div class=\"details\">";

    output << "<section><h3>Nachweise</h3>" << links(item);
    if (!item.implementations.empty()) {
        output << "<ul class=\"evidence\">";
        for (const auto& implementation : item.implementations) {
            output << "<li>✓ <code>" << escape(implementation.file) << ':'
                   << implementation.start_line << '-' << implementation.end_line
                   << "</code> <span class=\"muted\">Git "
                   << escape(implementation.git_hash) << "</span></li>";
        }
        output << "</ul>";
    }
    if (!item.test.status.empty()) {
        output << "<p>Testergebnis: "
               << state_badge(item.test.status,
                              item.test.status == "Passed" ? "good" : "bad")
               << " <span class=\"muted\">" << escape(item.test.source)
               << " · " << escape(item.test.timestamp) << "</span></p>";
    }
    output << "</section>";

    output << "<details class=\"audit\"><summary>Audit-Historie ("
           << history.size() << ")</summary><div class=\"table-wrap\"><table>"
           << "<thead><tr><th>Zeit</th><th>Aktion</th><th>Benutzer</th>"
              "<th>Grund</th></tr></thead><tbody>";
    for (const auto& entry : history) {
        output << "<tr><td>" << escape(entry.timestamp) << "</td><td>"
               << escape(entry.action) << "</td><td>" << escape(entry.actor)
               << "</td><td>" << escape(entry.reason) << "</td></tr>";
    }
    output << "</tbody></table></div></details></div></details></article>";
    return output.str();
}

std::string render(
    const Items& items,
    const capcom::audit::HistoryService& audit) {
    std::size_t complete_chains = 0;
    std::size_t root_chains = 0;
    std::size_t implemented = 0;
    std::size_t passed = 0;
    for (const auto& [uid, item] : items) {
        (void)uid;
        if (!item.implementations.empty()) ++implemented;
        if (item.test.status == "Passed") ++passed;
        if (item.type == "USR" && item.parents.empty()) {
            ++root_chains;
            if (chain_complete(item.uid, items, {})) ++complete_chains;
        }
    }

    std::ostringstream cards;
    for (const auto& [uid, item] : items) {
        cards << card(item, items, audit.entries(item.file, uid));
    }

    std::ostringstream html;
    html << R"HTML(<!doctype html><html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Captain's Log Report</title><script src="https://cdn.tailwindcss.com"></script>
<style>
:root{color-scheme:dark;--bg:#020617;--panel:#0f172a;--line:#334155;--text:#e2e8f0;--muted:#94a3b8;--cyan:#67e8f9}
*{box-sizing:border-box}html{scroll-behavior:smooth}body{margin:0;background:var(--bg);color:var(--text);font:14px system-ui,Segoe UI,sans-serif}a{color:var(--cyan);text-decoration:none}.header{padding:20px 24px;border-bottom:1px solid var(--line);background:#0f172a}.header h1{margin:0}.muted{color:var(--muted)}.stats{display:grid;grid-template-columns:repeat(4,minmax(120px,1fr));gap:12px;margin-top:16px}.stat,.sidebar,.card{background:var(--panel);border:1px solid var(--line);border-radius:12px}.stat{padding:14px}.stat strong{display:block;font-size:22px}.layout{display:grid;grid-template-columns:minmax(280px,360px) 1fr;gap:18px;padding:18px}.sidebar{position:sticky;top:18px;align-self:start;padding:15px;max-height:calc(100vh - 36px);overflow:auto}.sidebar input{width:100%;padding:10px;background:#020617;border:1px solid var(--line);border-radius:8px;color:var(--text)}.tree,.tree ul{list-style:none;padding-left:14px}.tree li{margin:8px 0}.tree summary,.item-summary,.audit summary{cursor:pointer}.card{margin-bottom:12px;scroll-margin-top:16px}.item-summary{display:flex;justify-content:space-between;gap:18px;padding:16px;list-style:none}.item-summary::-webkit-details-marker{display:none}.uid,code{font-family:Consolas,monospace;color:var(--cyan)}h2{font-size:18px;margin:3px 0}.preview{margin:5px 0 0;color:#cbd5e1;line-height:1.45}.signals{display:flex;gap:7px;align-items:flex-start;justify-content:flex-end;flex-wrap:wrap}.badge{display:inline-block;padding:3px 8px;border-radius:999px;font-size:12px;font-weight:700;white-space:nowrap}.good{background:#14532d;color:#bbf7d0}.warn{background:#713f12;color:#fde68a}.bad{background:#7f1d1d;color:#fecaca}.neutral{background:#1e293b;color:#cbd5e1}.details{border-top:1px solid var(--line);padding:0 16px 16px}.details h3{margin:16px 0 8px}.evidence{padding-left:18px}.audit{margin-top:14px}.table-wrap{overflow:auto}table{width:100%;border-collapse:collapse}th,td{text-align:left;padding:8px;border-bottom:1px solid #1e293b}.hidden{display:none!important}.bad-text{color:#fca5a5}
@media(max-width:900px){.layout{grid-template-columns:1fr}.sidebar{position:static;max-height:none}.stats{grid-template-columns:repeat(2,1fr)}.item-summary{display:block}.signals{justify-content:flex-start;margin-top:10px}}
</style></head><body><header class="header"><h1>Captain's Log Traceability Report</h1><p class="muted">Erzeugt: )HTML"
         << escape(generated_at()) << " · Audit-Signaturen verifiziert</p>"
         << "<div class=\"stats\"><div class=\"stat\"><strong>" << items.size()
         << "</strong><span class=\"muted\">Objekte</span></div>"
         << "<div class=\"stat\"><strong>" << implemented
         << "</strong><span class=\"muted\">mit Code</span></div>"
         << "<div class=\"stat\"><strong>" << passed
         << "</strong><span class=\"muted\">Tests Passed</span></div>"
         << "<div class=\"stat\"><strong>" << complete_chains << '/' << root_chains
         << "</strong><span class=\"muted\">gültige USR-Ketten</span></div></div></header>"
         << "<div class=\"layout\"><aside class=\"sidebar\"><input id=\"search\" "
            "type=\"search\" placeholder=\"UID, Titel oder Text suchen...\">"
            "<h3>Anforderungsbaum</h3>"
         << tree(items) << "</aside><main>" << cards.str() << R"HTML(</main></div>
<script>
const input=document.getElementById('search');
input.addEventListener('input',()=>{const q=input.value.toLocaleLowerCase('de');document.querySelectorAll('.requirement').forEach(card=>card.classList.toggle('hidden',!card.dataset.search.toLocaleLowerCase('de').includes(q)));});
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
    const capcom::audit::HistoryService audit{identity};
    audit.verify_project(project);
    const auto items = capcom::yaml::YamlStore{project}.load_all();
    if (items.empty()) throw std::runtime_error("No requirements found.");
    const auto report = project / "report.html";
    write_atomic(report, render(items, audit));
    std::cout << "Published compact verified report: " << report.string() << '\n';
    return 0;
}

} // namespace capcom::commands
