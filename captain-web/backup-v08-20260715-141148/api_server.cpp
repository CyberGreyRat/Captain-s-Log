#include "captain/api/api_server.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#endif
namespace captain::api {
namespace {
using json = nlohmann::json;
void reply(httplib::Response& response, const json& value, int status = 200) {
    response.status = status;
    response.set_content(value.dump(), "application/json; charset=utf-8");
}
void fail(httplib::Response& response, int status, const std::string& message) {
    reply(response, {{"error", {{"message", message}}}}, status);
}
std::string read_binary(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    return {std::istreambuf_iterator<char>(input), {}};
}
std::string now() {
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
std::vector<std::string> strings(const json& value, const char* key) {
    return value.contains(key)
        ? value.at(key).get<std::vector<std::string>>()
        : std::vector<std::string>{};
}
json command_json(const CommandResult& result) {
    return {{"ok", result.exit_code == 0},
            {"exit_code", result.exit_code},
            {"output", result.output}};
}
json read_events(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) return json::array();
    try { return json::parse(input); }
    catch (...) { return json::array(); }
}
void write_events(const std::filesystem::path& path, const json& events) {
    std::filesystem::create_directories(path.parent_path());
    auto temporary = path;
    temporary += ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("Cannot write web audit history.");
    output << events.dump(2);
    output.close();
    std::error_code error;
    std::filesystem::remove(path, error);
    error.clear();
    std::filesystem::rename(temporary, path, error);
    if (error) throw std::runtime_error("Cannot replace web audit history.");
}
}
ApiServer::ApiServer(
    repository::HybridRepository& repository,
    sync::SyncService& sync,
    std::filesystem::path project,
    std::filesystem::path web,
    std::filesystem::path cap_executable)
    : repository_(repository),
      sync_(sync),
      project_(std::move(project)),
      web_(std::move(web)),
      cli_(project_, std::move(cap_executable)) {}
std::string ApiServer::user() const {
#ifdef _WIN32
    char value[256]{};
    DWORD size = sizeof(value);
    return GetUserNameA(value, &size) ? value : "web-user";
#else
    return "web-user";
#endif
}
std::string ApiServer::host() const {
#ifdef _WIN32
    char value[256]{};
    DWORD size = sizeof(value);
    return GetComputerNameA(value, &size) ? value : "localhost";
#else
    return "localhost";
#endif
}
void ApiServer::audit(
    const std::string& action,
    const std::string& object_uid,
    const std::string& detail) const {
    const auto path = project_ / ".captain" / "web-audit.json";
    auto events = read_events(path);
    events.push_back({
        {"timestamp", now()}, {"action", action},
        {"object_uid", object_uid}, {"actor", user()},
        {"host", host()}, {"source", "Captain Web"},
        {"detail", detail}});
    write_events(path, events);
}
void ApiServer::routes() {
    server_.Get("/", [this](const auto&, auto& response) {
        response.set_content(read_binary(web_ / "index.html"),
                             "text/html; charset=utf-8");
    });
    server_.Get("/logo.png", [this](const auto&, auto& response) {
        response.set_content(read_binary(web_ / "logo.png"), "image/png");
    });
    server_.Get("/api/v1/session", [this](const auto&, auto& response) {
        reply(response, {{"user", user()}, {"host", host()},
                         {"roles", {"Developer"}}});
    });
    server_.Get("/api/v1/requirements", [this](const auto&, auto& response) {
        try { reply(response, {{"items", repository_.all()}}); }
        catch (const std::exception& error) { fail(response, 500, error.what()); }
    });
    server_.Post("/api/v1/requirements/create",
        [this](const auto& request, auto& response) {
        try {
            const auto body = json::parse(request.body);
            domain::CreateRequirement value{
                body.value("type", ""), body.value("title", ""),
                body.value("text", ""), body.value("rationale", ""),
                strings(body, "parents"), strings(body, "children"),
                user(), host(), "Captain Web"};
            auto created = repository_.create(value);
            const auto signed_result = cli_.sign(
                created.uid,
                "Created with Captain Web by " + user() + " on " + host());
            audit("REQUIREMENT_CREATED", created.uid,
                  "Requirement created and links assigned.");
            reply(response,
                  {{"item", created}, {"audit", command_json(signed_result)}},
                  201);
        } catch (const std::exception& error) {
            fail(response, 422, error.what());
        }
    });
    server_.Post(R"(/api/v1/requirements/([A-Za-z0-9_-]+)/update)",
        [this](const auto& request, auto& response) {
        try {
            const auto uid = request.matches[1].str();
            auto current = repository_.all();
            auto found = std::find_if(current.begin(), current.end(),
                [&](const auto& item) { return item.uid == uid; });
            if (found == current.end()) throw std::runtime_error("UID not found.");
            const auto body = json::parse(request.body);
            found->title = body.value("title", found->title);
            found->text = body.value("text", found->text);
            found->rationale = body.value("rationale", found->rationale);
            found->updated_by = user();
            auto updated = repository_.update(*found);
            const auto signed_result = cli_.sign(
                uid, "Edited with Captain Web by " + user());
            audit("REQUIREMENT_UPDATED", uid,
                  body.value("reason", "Requirement edited in browser."));
            reply(response,
                  {{"item", updated}, {"audit", command_json(signed_result)}});
        } catch (const std::exception& error) {
            fail(response, 422, error.what());
        }
    });
    server_.Get(R"(/api/v1/requirements/([A-Za-z0-9_-]+)/comments)",
        [this](const auto& request, auto& response) {
            reply(response, {{"comments",
                repository_.comments(request.matches[1].str())}});
        });
    server_.Post(R"(/api/v1/requirements/([A-Za-z0-9_-]+)/comment)",
        [this](const auto& request, auto& response) {
            const auto body = json::parse(request.body);
            const auto uid = request.matches[1].str();
            auto comment = repository_.add_comment(
                uid, user(), body.value("text", ""));
            audit("COMMENT_ADDED", uid, comment.text);
            reply(response, {{"comment", comment}}, 201);
        });
    server_.Get(R"(/api/v1/requirements/([A-Za-z0-9_-]+)/files)",
        [this](const auto& request, auto& response) {
            reply(response, {{"file_links",
                repository_.files(request.matches[1].str())}});
        });
    server_.Get("/api/v1/documents", [this](const auto&, auto& response) {
        reply(response, {{"documents", repository_.all_files()}});
    });
    server_.Post("/api/v1/documents/link",
        [this](const auto& request, auto& response) {
        try {
            const auto body = json::parse(request.body);
            domain::FileLink value;
            value.requirement_uid = body.value("requirement_uid", "");
            value.kind = body.value("kind", "web_url");
            value.label = body.value("label", "");
            value.location = body.value("location", "");
            value.version = body.value("version", "");
            value.created_by = user();
            value.global = body.value("global", false);
            if (value.label.empty() || value.location.empty()) {
                throw std::runtime_error("Bezeichnung und Adresse sind erforderlich.");
            }
            auto created = repository_.add_file(value);
            audit("DOCUMENT_LINKED", value.requirement_uid,
                  value.label + " -> " + value.location);
            reply(response, {{"document", created}}, 201);
        } catch (const std::exception& error) {
            fail(response, 422, error.what());
        }
    });
    server_.Post(R"(/api/v1/documents/([A-Za-z0-9_-]+)/update)",
        [this](const auto& request, auto& response) {
        try {
            const auto body = json::parse(request.body);
            domain::FileLink value;
            value.id = request.matches[1].str();
            value.requirement_uid = body.value("requirement_uid", "");
            value.kind = body.value("kind", "web_url");
            value.label = body.value("label", "");
            value.location = body.value("location", "");
            value.version = body.value("version", "");
            value.hash = body.value("hash", "");
            value.created_by = user();
            value.global = value.requirement_uid.empty();
            if (value.label.empty() || value.location.empty()) {
                throw std::runtime_error("Bezeichnung und Adresse sind erforderlich.");
            }
            auto updated = repository_.update_file(value);
            audit("DOCUMENT_UPDATED", value.requirement_uid,
                  value.label + " -> " + value.location);
            reply(response, {{"document", updated}});
        } catch (const std::exception& error) {
            fail(response, 422, error.what());
        }
    });
    server_.Get("/api/v1/history", [this](const auto&, auto& response) {
        reply(response, {
            {"captain_log", read_binary(project_ / "reqs" / "captainslog.yml")},
            {"web_events", read_events(project_ / ".captain" / "web-audit.json")}});
    });
    server_.Post("/api/v1/sync/pull", [this](const auto&, auto& response) {
        auto result = sync_.pull();
        audit("SYNC_PULLED", "PROJECT",
              std::to_string(result.pulled) + " objects pulled.");
        reply(response, {{"pulled", result.pulled}});
    });
    server_.Post("/api/v1/sync/push", [this](const auto& request, auto& response) {
        const auto body = json::parse(request.body.empty() ? "{}" : request.body);
        const auto revision = body.value("git_commit", "working-tree");
        auto result = sync_.push(revision, user());
        audit("SYNC_PUSHED", "PROJECT",
              std::to_string(result.pushed) + " objects pushed at " + revision);
        reply(response, {{"pushed", result.pushed}});
    });
    server_.Post(R"(/api/v1/requirements/([A-Za-z0-9_-]+)/archive)",
        [this](const auto& request, auto& response) {
        try {
            const auto uid = request.matches[1].str();
            const auto body = json::parse(request.body);
            const auto reason = body.value("reason", "");
            if (reason.empty()) throw std::runtime_error("Archivierungsgrund ist erforderlich.");
            auto values = repository_.all();
            auto found = std::find_if(values.begin(), values.end(),
                [&](const auto& item) { return item.uid == uid; });
            if (found == values.end()) throw std::runtime_error("UID nicht gefunden.");
            found->status = "Archived";
            found->updated_by = user();
            const auto saved = repository_.update(*found);
            const auto signed_result = cli_.sign(uid, "Archived: " + reason);
            audit("REQUIREMENT_ARCHIVED", uid, reason);
            reply(response, {{"item", saved}, {"audit", command_json(signed_result)}});
        } catch (const std::exception& error) { fail(response, 422, error.what()); }
    });
    server_.Post(R"(/api/v1/requirements/([A-Za-z0-9_-]+)/restore)",
        [this](const auto& request, auto& response) {
        try {
            const auto uid = request.matches[1].str();
            const auto body = json::parse(request.body);
            const auto reason = body.value("reason", "Wiederhergestellt");
            auto values = repository_.all();
            auto found = std::find_if(values.begin(), values.end(),
                [&](const auto& item) { return item.uid == uid; });
            if (found == values.end()) throw std::runtime_error("UID nicht gefunden.");
            found->status = "Draft";
            found->updated_by = user();
            const auto saved = repository_.update(*found);
            const auto signed_result = cli_.sign(uid, "Restored: " + reason);
            audit("REQUIREMENT_RESTORED", uid, reason);
            reply(response, {{"item", saved}, {"audit", command_json(signed_result)}});
        } catch (const std::exception& error) { fail(response, 422, error.what()); }
    });
    server_.Post(R"(/api/v1/documents/([A-Za-z0-9_-]+)/archive)",
        [this](const auto& request, auto& response) {
        try {
            const auto id = request.matches[1].str();
            const auto body = json::parse(request.body);
            const auto reason = body.value("reason", "");
            if (reason.empty()) throw std::runtime_error("Archivierungsgrund ist erforderlich.");
            auto values = repository_.all_files();
            auto found = std::find_if(values.begin(), values.end(),
                [&](const auto& item) { return item.id == id; });
            if (found == values.end()) throw std::runtime_error("Dokumentlink nicht gefunden.");
            if (found->kind.rfind("archived:", 0) != 0) found->kind = "archived:" + found->kind;
            found->created_by = user();
            const auto saved = repository_.update_file(*found);
            audit("DOCUMENT_ARCHIVED", found->requirement_uid, reason);
            reply(response, {{"document", saved}});
        } catch (const std::exception& error) { fail(response, 422, error.what()); }
    });
    server_.Post(R"(/api/v1/documents/([A-Za-z0-9_-]+)/restore)",
        [this](const auto& request, auto& response) {
        try {
            const auto id = request.matches[1].str();
            const auto body = json::parse(request.body);
            const auto reason = body.value("reason", "Wiederhergestellt");
            auto values = repository_.all_files();
            auto found = std::find_if(values.begin(), values.end(),
                [&](const auto& item) { return item.id == id; });
            if (found == values.end()) throw std::runtime_error("Dokumentlink nicht gefunden.");
            const std::string prefix = "archived:";
            if (found->kind.rfind(prefix, 0) == 0) found->kind = found->kind.substr(prefix.size());
            found->created_by = user();
            const auto saved = repository_.update_file(*found);
            audit("DOCUMENT_RESTORED", found->requirement_uid, reason);
            reply(response, {{"document", saved}});
        } catch (const std::exception& error) { fail(response, 422, error.what()); }
    });
    server_.Get("/api/v1/project/status", [this](const auto&, auto& response) {
        reply(response, command_json(cli_.status()));
    });
    server_.Post("/api/v1/project/scan", [this](const auto&, auto& response) {
        auto result = cli_.scan(); audit("PROJECT_SCANNED", "PROJECT", result.output);
        reply(response, command_json(result));
    });
    server_.Post("/api/v1/project/verify", [this](const auto&, auto& response) {
        auto result = cli_.verify(); audit("PROJECT_VERIFIED", "PROJECT", result.output);
        reply(response, command_json(result));
    });
    server_.Post("/api/v1/project/validate", [this](const auto&, auto& response) {
        auto result = cli_.validate(); audit("PROJECT_VALIDATED", "PROJECT", result.output);
        reply(response, command_json(result));
    });
    server_.Post("/api/v1/project/publish", [this](const auto&, auto& response) {
        auto result = cli_.publish(); audit("REPORT_PUBLISHED", "PROJECT", result.output);
        reply(response, command_json(result));
    });
    server_.Get("/api/v1/project/tree", [this](const auto&, auto& response) {
        reply(response, command_json(cli_.tree()));
    });
}
void ApiServer::listen(const std::string& host_name, int port) {
    routes();
    server_.listen(host_name, port);
}
}

