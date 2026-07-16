#include "capcom/commands/publish_command.hpp"

#include "capcom/audit/history_service.hpp"
#include "capcom/identity/identity_manager.hpp"
#include "capcom/yaml/yaml_store.hpp"

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
#ifdef _WIN32
#include <windows.h>
#endif

namespace capcom::commands {
namespace {

using Items = std::map<std::string, capcom::yaml::Item>;

bool is_test_type(const std::string& type) {
    return type == "TEST" || type == "UT" || type == "IT" ||
           type == "ST" || type == "AT";
}

bool is_base_complete(const capcom::yaml::Item& item) {
    if (item.uid.empty() || item.title.empty() || item.text.empty() ||
        item.rationale.empty()) {
        return false;
    }

    if ((item.type == "SRS" || item.type == "SEC") &&
        item.implementations.empty()) {
        return false;
    }

    if (is_test_type(item.type) &&
        (item.implementations.empty() || item.test.status != "Passed")) {
        return false;
    }

    return true;
}

bool is_chain_complete(
    const std::string& uid,
    const Items& items,
    std::set<std::string> path) {
    const auto found = items.find(uid);
    if (found == items.end() || !path.insert(uid).second ||
        !is_base_complete(found->second)) {
        return false;
    }

    const auto& item = found->second;
    if (item.type == "USR" || item.type == "SYS" ||
        item.type == "SRS" || item.type == "SEC") {
        if (item.children.empty()) {
            return false;
        }

        for (const auto& child_uid : item.children) {
            if (!is_chain_complete(child_uid, items, path)) {
                return false;
            }
        }
    }

    return true;
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

std::string badge(const std::string& text, const std::string& css_class) {
    return "<span class=\"status-badge " + css_class + "\">" +
           html_escape(text) + "</span>";
}

std::string lifecycle_badge(const std::string& status) {
    if (status == "Approved") return badge(status, "status-approved");
    if (status == "Rejected") return badge(status, "status-failed");
    if (status == "In Review") return badge(status, "status-review");
    return badge(status, "status-draft");
}

std::string item_signals(const capcom::yaml::Item& item) {
    std::ostringstream output;
    output << lifecycle_badge(item.status);

    if (!item.implementations.empty()) {
        output << badge("Code verknüpft", "status-valid");
    } else if (item.type == "SRS" || item.type == "SEC" ||
               is_test_type(item.type)) {
        output << badge("Code fehlt", "status-open");
    }

    if (is_test_type(item.type)) {
        if (item.test.status == "Passed") {
            output << badge("Passed", "status-passed");
        } else if (item.test.status == "Failed") {
            output << badge("Failed", "status-failed");
        } else {
            output << badge("Nicht ausgeführt", "status-open");
        }
    }

    return output.str();
}

std::string render_tree_node(
    const std::string& uid,
    const Items& items,
    std::set<std::string> path) {
    const auto found = items.find(uid);
    if (found == items.end()) {
        return "<li class=\"text-red-700\">" + html_escape(uid) +
               " fehlt</li>";
    }

    if (!path.insert(uid).second) {
        return "<li><a class=\"text-blue-800 hover:underline\" href=\"#" +
               html_escape(uid) + "\">" + html_escape(uid) +
               " (Referenz)</a></li>";
    }

    const auto& item = found->second;
    const bool complete = is_chain_complete(uid, items, {});

    std::ostringstream output;
    output << "<li class=\"tree-node\"><details><summary class=\"flex "
              "cursor-pointer items-start gap-2 rounded-md px-2 py-1.5 "
              "hover:bg-slate-100\">"
           << "<span class=\"chevron mt-0.5 text-slate-400\">›</span>"
           << "<span class=\"min-w-0 flex-1\"><a class=\"font-mono "
              "font-bold text-blue-900 hover:underline\" href=\"#"
           << html_escape(uid) << "\">" << html_escape(uid) << "</a> "
           << "<span class=\"text-slate-700\">" << html_escape(item.title)
           << "</span></span>"
           << (complete
                   ? badge("Kette gültig", "status-valid")
                   : badge("Kette offen", "status-open"))
           << "</summary>";

    if (!item.children.empty()) {
        output << "<ul>";
        for (const auto& child_uid : item.children) {
            output << render_tree_node(child_uid, items, path);
        }
        output << "</ul>";
    }

    output << "</details></li>";
    return output.str();
}

std::string render_tree(const Items& items) {
    std::ostringstream output;

    for (const auto& [uid, item] : items) {
        if (item.parents.empty()) {
            output << render_tree_node(uid, items, {});
        }
    }

    return output.str();
}

std::string render_links(const capcom::yaml::Item& item) {
    std::ostringstream output;

    if (!item.parents.empty()) {
        output << "<p><span class=\"font-semibold text-slate-600\">Parent:</span> ";
        for (const auto& uid : item.parents) {
            output << "<a class=\"font-mono text-blue-800 hover:underline\" "
                      "href=\"#"
                   << html_escape(uid) << "\">" << html_escape(uid)
                   << "</a> ";
        }
        output << "</p>";
    }

    if (!item.children.empty()) {
        output << "<p><span class=\"font-semibold text-slate-600\">Children:</span> ";
        for (const auto& uid : item.children) {
            output << "<a class=\"font-mono text-blue-800 hover:underline\" "
                      "href=\"#"
                   << html_escape(uid) << "\">" << html_escape(uid)
                   << "</a> ";
        }
        output << "</p>";
    }

    return output.str();
}

std::string render_card(
    const capcom::yaml::Item& item,
    const Items& items,
    const std::vector<capcom::audit::AuditEntry>& history) {
    const bool complete = is_chain_complete(item.uid, items, {});
    std::ostringstream output;

    output << "<article id=\"" << html_escape(item.uid)
           << "\" class=\"requirement-card rounded-lg border border-slate-200 "
              "bg-white shadow-md\" data-search=\""
           << html_escape(item.uid + " " + item.title + " " + item.text)
           << "\"><details>"
           << "<summary class=\"flex cursor-pointer list-none flex-col gap-4 "
              "p-5 md:flex-row md:items-start md:justify-between\">"
           << "<div class=\"min-w-0\">"
           << "<div class=\"font-mono text-sm font-bold text-blue-900\">"
           << html_escape(item.uid) << "</div>"
           << "<h2 class=\"mt-1 text-lg font-bold text-slate-900\">"
           << html_escape(item.title) << "</h2>"
           << "<p class=\"mt-2 leading-6 text-slate-700\">"
           << html_escape(item.text) << "</p></div>"
           << "<div class=\"flex max-w-md flex-wrap gap-2 md:justify-end\">"
           << item_signals(item)
           << (complete
                   ? badge("Kette gültig", "status-valid")
                   : badge("Kette unvollständig", "status-open"))
           << "</div></summary>"
           << "<div class=\"border-t border-slate-200 px-5 pb-5\">"
           << "<section class=\"pt-5\"><h3 class=\"text-sm font-bold "
              "uppercase tracking-wide text-blue-900\">Nachweise</h3>"
           << "<div class=\"mt-3 space-y-1 text-sm\">"
           << render_links(item) << "</div>";

    if (!item.implementations.empty()) {
        output << "<ul class=\"mt-4 space-y-2\">";
        for (const auto& implementation : item.implementations) {
            output << "<li class=\"rounded-md border border-slate-200 bg-slate-50 "
                      "px-3 py-2 text-sm\">"
                   << "<span class=\"font-semibold text-green-700\">✓</span> "
                   << "<code class=\"text-blue-900\">"
                   << html_escape(implementation.file) << ':'
                   << implementation.start_line << '-' << implementation.end_line
                   << "</code> <span class=\"text-slate-500\">Git "
                   << html_escape(implementation.git_hash) << "</span></li>";
        }
        output << "</ul>";
    }

    if (!item.test.status.empty()) {
        output << "<p class=\"mt-4 text-sm\">Testergebnis: "
               << (item.test.status == "Passed"
                       ? badge("Passed", "status-passed")
                       : badge(item.test.status, "status-failed"))
               << " <span class=\"text-slate-500\">"
               << html_escape(item.test.source) << " · "
               << html_escape(item.test.timestamp) << "</span></p>";
    }

    output << "</section>"
           << "<details class=\"audit-history mt-5 rounded-md border "
              "border-slate-200\"><summary class=\"cursor-pointer bg-slate-50 "
              "px-4 py-3 text-sm font-semibold text-slate-700\">"
           << "Audit-Historie (" << history.size() << ")</summary>"
           << "<div class=\"overflow-x-auto\"><table class=\"min-w-full "
              "divide-y divide-slate-200 text-left text-sm\">"
           << "<thead class=\"bg-slate-100 text-xs uppercase tracking-wide "
              "text-slate-600\"><tr><th class=\"px-4 py-3\">Zeit</th>"
              "<th class=\"px-4 py-3\">Aktion</th>"
              "<th class=\"px-4 py-3\">Benutzer</th>"
              "<th class=\"px-4 py-3\">Grund</th></tr></thead>"
              "<tbody class=\"divide-y divide-slate-100 bg-white\">";

    for (const auto& entry : history) {
        output << "<tr><td class=\"whitespace-nowrap px-4 py-3 text-slate-600\">"
               << html_escape(entry.timestamp)
               << "</td><td class=\"px-4 py-3 font-mono text-blue-900\">"
               << html_escape(entry.action)
               << "</td><td class=\"px-4 py-3 text-slate-700\">"
               << html_escape(entry.actor)
               << "</td><td class=\"px-4 py-3 text-slate-700\">"
               << html_escape(entry.reason) << "</td></tr>";
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
            if (is_chain_complete(item.uid, items, {})) {
                ++complete_chains;
            }
        }
    }

    std::ostringstream cards;
    for (const auto& [uid, item] : items) {
        cards << render_card(item, items, audit.entries(item.file, uid));
    }

    const std::string html_template = R"HTML(
<!DOCTYPE html>
<html lang="de">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Captain's Log - Traceability Report</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <script>
        tailwind.config = {
            theme: {
                extend: {
                    colors: {
                        aerospace: {
                            900: "#0b2d52",
                            950: "#061e38"
                        }
                    }
                }
            }
        };
    </script>
    <style>
        html { scroll-behavior: smooth; }
        body { font-family: ui-sans-serif, system-ui, -apple-system, "Segoe UI", sans-serif; }
        .requirement-card { scroll-margin-top: 2rem; }
        .requirement-card summary::-webkit-details-marker,
        .tree-node summary::-webkit-details-marker,
        .audit-history summary::-webkit-details-marker { display: none; }
        .tree-list, .tree-list ul { list-style: none; margin: 0; padding-left: 1.15rem; }
        .tree-list > li { padding-left: 0; }
        .tree-list li { margin: 0.35rem 0; }
        .status-badge { display: inline-flex; align-items: center; border-radius: 9999px; padding: 0.25rem 0.65rem; font-size: 0.75rem; font-weight: 700; white-space: nowrap; }
        .status-passed, .status-approved, .status-valid { border: 1px solid #bbf7d0; background: #f0fdf4; color: #15803d; }
        .status-draft, .status-review, .status-open { border: 1px solid #fed7aa; background: #fff7ed; color: #c2410c; }
        .status-failed { border: 1px solid #fecaca; background: #fef2f2; color: #b91c1c; }
        .chevron { transition: transform 150ms ease; }
        details[open] > summary .chevron { transform: rotate(90deg); }
        .hidden-by-search { display: none !important; }
        @media print {
            body { background: white !important; }
            aside { display: none !important; }
            .report-layout { display: block !important; }
            .requirement-card { break-inside: avoid; box-shadow: none !important; }
        }
    </style>
</head>
<body class="min-h-screen bg-slate-50 text-slate-800 antialiased">
    <header class="bg-aerospace-900 text-white shadow-lg">
        <div class="mx-auto flex max-w-screen-2xl items-center gap-5 px-6 py-6">
            <img src="../captains-log-logo.png" class="h-16 w-auto" alt="Captain's Log Logo">
            <div>
                <p class="text-xs font-semibold uppercase tracking-[0.22em] text-blue-200">Requirements · Risk · Verification</p>
                <h1 class="mt-1 text-2xl font-bold tracking-tight md:text-3xl">Captain's Log - Traceability Report</h1>
                <p class="mt-1 text-sm text-blue-100">Generiert: {{GENERATED_AT}} · Audit-Signaturen verifiziert</p>
            </div>
        </div>
    </header>

    <main class="mx-auto max-w-screen-2xl px-6 py-8">
        <section class="mb-8 grid gap-4 sm:grid-cols-2 xl:grid-cols-4">
            <div class="rounded-lg border border-slate-200 bg-white p-5 shadow-sm"><p class="text-xs font-semibold uppercase tracking-wider text-slate-500">Objekte</p><p class="mt-2 text-3xl font-bold text-blue-900">{{ITEM_COUNT}}</p></div>
            <div class="rounded-lg border border-slate-200 bg-white p-5 shadow-sm"><p class="text-xs font-semibold uppercase tracking-wider text-slate-500">Code-Verknüpfungen</p><p class="mt-2 text-3xl font-bold text-blue-900">{{IMPLEMENTED_COUNT}}</p></div>
            <div class="rounded-lg border border-slate-200 bg-white p-5 shadow-sm"><p class="text-xs font-semibold uppercase tracking-wider text-slate-500">Bestandene Tests</p><p class="mt-2 text-3xl font-bold text-green-700">{{PASSED_TEST_COUNT}}</p></div>
            <div class="rounded-lg border border-slate-200 bg-white p-5 shadow-sm"><p class="text-xs font-semibold uppercase tracking-wider text-slate-500">Gültige Ketten</p><p class="mt-2 text-3xl font-bold text-green-700">{{VALID_CHAIN_COUNT}} / {{CHAIN_COUNT}}</p></div>
        </section>

        <div class="report-layout grid gap-6 lg:grid-cols-[22rem_minmax(0,1fr)]">
            <aside class="self-start rounded-lg border border-slate-200 bg-white shadow-md lg:sticky lg:top-6">
                <div class="border-b border-slate-200 p-5">
                    <h2 class="text-lg font-bold text-blue-900">Requirements Tree</h2>
                    <p class="mt-1 text-sm text-slate-500">Navigation durch die Traceability-Struktur</p>
                </div>
                <div class="p-5">
                    <input id="requirement-search" type="search" placeholder="UID, Titel oder Text suchen" class="mb-5 w-full rounded-md border border-slate-300 bg-white px-3 py-2.5 text-sm outline-none focus:border-blue-700 focus:ring-2 focus:ring-blue-100">
                    <nav class="max-h-[65vh] overflow-auto pr-2 text-sm text-slate-700">
                        <ul class="tree-list">{{REQUIREMENTS_TREE}}</ul>
                    </nav>
                </div>
            </aside>

            <section class="min-w-0 space-y-4">{{REQUIREMENT_CARDS}}</section>
        </div>
    </main>

    <footer class="mt-10 border-t border-slate-200 bg-white">
        <div class="mx-auto flex max-w-screen-2xl justify-between px-6 py-5 text-xs text-slate-500">
            <p>Captain's Log · Requirements and Traceability</p>
            <p>Berichtsintegrität: <span class="font-semibold text-green-700">kryptografisch verifiziert</span></p>
        </div>
    </footer>

    <script>
        const searchInput = document.getElementById("requirement-search");
        searchInput.addEventListener("input", () => {
            const query = searchInput.value.trim().toLocaleLowerCase("de");
            document.querySelectorAll(".requirement-card").forEach((card) => {
                const text = (card.dataset.search || "").toLocaleLowerCase("de");
                card.classList.toggle("hidden-by-search", query.length > 0 && !text.includes(query));
            });
        });
    </script>
</body>
</html>
)HTML";

    std::string html = html_template;

    const auto replace_all = [&html](
                                 const std::string& placeholder,
                                 const std::string& value) {
        std::size_t position = 0;
        while ((position = html.find(placeholder, position)) !=
               std::string::npos) {
            html.replace(position, placeholder.size(), value);
            position += value.size();
        }
    };

    replace_all("{{GENERATED_AT}}", generated_at_utc());
    replace_all("{{ITEM_COUNT}}", std::to_string(items.size()));
    replace_all("{{IMPLEMENTED_COUNT}}", std::to_string(implemented));
    replace_all("{{PASSED_TEST_COUNT}}", std::to_string(passed));
    replace_all("{{VALID_CHAIN_COUNT}}", std::to_string(complete_chains));
    replace_all("{{CHAIN_COUNT}}", std::to_string(root_chains));
    replace_all("{{REQUIREMENTS_TREE}}", render_tree(items));
    replace_all("{{REQUIREMENT_CARDS}}", cards.str());

    return html;
}

void write_atomic(
    const std::filesystem::path& file,
    const std::string& content) {
    auto temporary = file;
    temporary += ".tmp";

    {
        std::ofstream output(
            temporary,
            std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("Cannot write " + temporary.string());
        }

        output.write(
            content.data(),
            static_cast<std::streamsize>(content.size()));
        if (!output) {
            throw std::runtime_error("Failed writing " + temporary.string());
        }
    }

    std::error_code error;
    std::filesystem::remove(file, error);
    error.clear();
    std::filesystem::rename(temporary, file, error);

    if (error) {
        throw std::runtime_error(
            "Cannot create report: " + error.message());
    }
}

} // namespace

void run_professional_publisher(
    const std::filesystem::path& project,
    const std::filesystem::path& html);
int PublishCommand::execute(const std::filesystem::path& project) const {
    const auto identity =
        capcom::identity::IdentityManager{}.load_or_create();
    const capcom::audit::HistoryService audit{identity};

    audit.verify_project(project);

    const auto items = capcom::yaml::YamlStore{project}.load_all();
    if (items.empty()) {
        throw std::runtime_error("No requirements found.");
    }

    const auto report = project / "report.html";
    write_atomic(report, render(items, audit));
    run_professional_publisher(project, report);

    std::cout << "Published verified report: "
              << report.string() << '\n';
    return 0;
}

#ifdef _WIN32
std::wstring quote_argument(const std::wstring& value) {
    std::wstring result = L"\"";
    for (const auto character : value) {
        if (character == L'\"') result += L"\\\"";
        else result += character;
    }
    result += L"\"";
    return result;
}

void run_professional_publisher(
    const std::filesystem::path& project,
    const std::filesystem::path& html) {
    std::vector<wchar_t> module(32768, L'\0');
    const auto length = GetModuleFileNameW(
        nullptr,
        module.data(),
        static_cast<DWORD>(module.size()));
    if (length == 0 || length >= module.size()) {
        throw std::runtime_error(
            "Cannot locate cap.exe for professional publishing.");
    }

    const auto script = std::filesystem::path{
        std::wstring(module.data(), length)}.parent_path() /
        L"Publish-CaptainsLog-Professional.ps1";
    if (!std::filesystem::exists(script)) {
        throw std::runtime_error(
            "Professional publisher missing: " + script.string());
    }

    const std::wstring command =
        L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File " +
        quote_argument(script.wstring()) +
        L" -Project " + quote_argument(project.wstring()) +
        L" -Html " + quote_argument(html.wstring());

    const int result = _wsystem(command.c_str());
    if (result != 0) {
        throw std::runtime_error(
            "Professional report package generation failed.");
    }
}
#else
void run_professional_publisher(
    const std::filesystem::path&,
    const std::filesystem::path&) {
    throw std::runtime_error(
        "Professional publishing currently requires Windows and Microsoft Word.");
}
#endif
} // namespace capcom::commands
