#include "capcom/audit/history_service.hpp"
#include "capcom/security/crypto.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace capcom::audit {
namespace {

std::string timestamp_utc() {
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

std::string yaml_quote(const std::string& value) {
    std::string output{"\""};
    for (const char character : value) {
        if (character == '\\' || character == '"') output.push_back('\\');
        if (character == '\n') output += "\\n";
        else if (character != '\r') output.push_back(character);
    }
    output.push_back('"');
    return output;
}

std::string yaml_unquote(std::string value) {
    const auto first = value.find_first_not_of(" \t");
    value = first == std::string::npos ? std::string{} : value.substr(first);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }
    return value;
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

void save_atomic(
    const std::filesystem::path& file,
    const std::vector<std::string>& lines) {
    auto temporary = file;
    temporary += ".audit.tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("Cannot write " + temporary.string());
        for (const auto& line : lines) output << line << '\n';
        if (!output) throw std::runtime_error("Failed writing " + temporary.string());
    }
    std::error_code error;
    std::filesystem::remove(file, error);
    error.clear();
    std::filesystem::rename(temporary, file, error);
    if (error) throw std::runtime_error("Cannot update audit history: " + error.message());
}

std::string content_without_history(const std::vector<std::string>& lines) {
    std::ostringstream output;
    bool inside_history = false;
    for (const auto& line : lines) {
        if (line.rfind("history:", 0) == 0) {
            inside_history = true;
            continue;
        }
        if (inside_history && !line.empty() && line.front() != ' ' && line.front() != '\t') {
            inside_history = false;
        }
        if (!inside_history) output << line << '\n';
    }
    return output.str();
}

std::string scalar_value(const std::string& line) {
    const auto colon = line.find(':');
    if (colon == std::string::npos) return {};
    return yaml_unquote(line.substr(colon + 1));
}

std::string canonical_payload(
    const std::string& uid,
    const std::string& time,
    const std::string& action,
    const std::string& actor,
    const std::string& host_id,
    const std::string& key_id,
    const std::string& content_hash,
    const std::string& previous_signature,
    const std::string& reason) {
    return
        "CAPTAINS-LOG-HISTORY-V2\n"
        "uid=" + uid + "\n"
        "timestamp=" + time + "\n"
        "action=" + action + "\n"
        "actor=" + actor + "\n"
        "host_id=" + host_id + "\n"
        "key_id=" + key_id + "\n"
        "content_hash=" + content_hash + "\n"
        "previous_signature=" + previous_signature + "\n"
        "reason=" + reason + "\n";
}

struct ParsedHistory {
    int count{};
    std::string latest_signature{"GENESIS"};
    std::string latest_content_hash;
};

ParsedHistory verify_chain(
    const std::vector<std::string>& lines,
    const std::string& uid) {
    ParsedHistory result{};
    std::map<std::string, std::string> entry;
    bool inside_history = false;

    auto verify_entry = [&]() {
        if (entry.empty()) return;
        ++result.count;
        const auto reason = entry.contains("reason") ? entry["reason"] : std::string{};
        const auto payload = canonical_payload(
            uid,
            entry["timestamp"],
            entry["action"],
            entry["actor"],
            entry["host_id"],
            entry["key_id"],
            entry["content_hash"],
            entry["previous_signature"],
            reason);

        if (entry["previous_signature"] != result.latest_signature ||
            !capcom::security::verify_rsa_pss_sha256(
                entry["public_key"], payload, entry["signature"])) {
            throw std::runtime_error(
                "SECURITY ALERT: Tampering detected in requirement " + uid +
                "! History entry signature invalid.");
        }
        result.latest_signature = entry["signature"];
        result.latest_content_hash = entry["content_hash"];
        entry.clear();
    };

    for (const auto& line : lines) {
        if (line.rfind("history:", 0) == 0) {
            inside_history = true;
            continue;
        }
        if (!inside_history) continue;
        if (!line.empty() && line.front() != ' ' && line.front() != '\t') break;

        auto trimmed = line;
        const auto first = trimmed.find_first_not_of(" \t-");
        if (first == std::string::npos) continue;
        trimmed.erase(0, first);
        const auto colon = trimmed.find(':');
        if (colon == std::string::npos) continue;
        const auto key = trimmed.substr(0, colon);
        if (key == "timestamp" && !entry.empty()) verify_entry();
        entry[key] = scalar_value(trimmed);
    }
    verify_entry();

    if (result.count == 0) {
        throw std::runtime_error(
            "SECURITY ALERT: Unsigned history in requirement " + uid);
    }
    return result;
}

} // namespace

HistoryService::HistoryService(capcom::identity::Identity identity)
    : identity_{std::move(identity)} {}

std::string HistoryService::current_content_hash(
    const std::filesystem::path& file) const {
    return capcom::security::sha256_hex(
        content_without_history(read_lines(file)));
}

std::string HistoryService::latest_signed_content_hash(
    const std::filesystem::path& file) const {
    std::ifstream input(file);
    std::string line;
    std::string latest;
    while (std::getline(input, line)) {
        auto trimmed = line;
        const auto first = trimmed.find_first_not_of(" \t-");
        if (first == std::string::npos) continue;
        trimmed.erase(0, first);
        if (trimmed.rfind("content_hash:", 0) == 0) latest = scalar_value(trimmed);
    }
    return latest;
}

bool HistoryService::has_unsigned_content_changes(
    const std::filesystem::path& file,
    const std::string& uid) const {
    verify_history_chain(file, uid);
    return current_content_hash(file) != latest_signed_content_hash(file);
}

void HistoryService::append(
    const std::filesystem::path& file,
    const std::string& uid,
    const std::string& action,
    const std::string& reason) const {
    auto lines = read_lines(file);
    std::string previous = "GENESIS";
    bool has_history = false;
    for (const auto& line : lines) {
        auto trimmed = line;
        const auto first = trimmed.find_first_not_of(" \t-");
        if (first == std::string::npos) continue;
        trimmed.erase(0, first);
        if (trimmed.rfind("signature:", 0) == 0) {
            previous = scalar_value(trimmed);
            has_history = true;
        }
    }
    if (has_history) verify_history_chain(file, uid);

    const auto time = timestamp_utc();
    const auto content_hash = capcom::security::sha256_hex(
        content_without_history(lines));
    const auto payload = canonical_payload(
        uid, time, action, identity_.full_name, identity_.host_id,
        identity_.key_id, content_hash, previous, reason);
    const auto signature = capcom::security::sign_rsa_pss_sha256(
        identity_.key_name, payload);

    auto history = std::find_if(lines.begin(), lines.end(), [](const auto& line) {
        return line.rfind("history:", 0) == 0;
    });
    if (history == lines.end()) {
        auto insertion = std::find_if(lines.begin(), lines.end(), [](const auto& line) {
            return line.rfind("integration_template:", 0) == 0;
        });
        history = lines.insert(insertion, "history:");
    }
    auto end = history + 1;
    while (end != lines.end() &&
           (end->empty() || end->front() == ' ' || end->front() == '\t')) {
        ++end;
    }

    const std::vector<std::string> block{
        "  - timestamp: " + yaml_quote(time),
        "    action: " + yaml_quote(action),
        "    actor: " + yaml_quote(identity_.full_name),
        "    host_id: " + yaml_quote(identity_.host_id),
        "    key_id: " + yaml_quote(identity_.key_id),
        "    algorithm: \"RSA-PSS-SHA256\"",
        "    public_key: " + yaml_quote(identity_.public_key_base64),
        "    content_hash: " + yaml_quote(content_hash),
        "    previous_signature: " + yaml_quote(previous),
        "    reason: " + yaml_quote(reason),
        "    signature: " + yaml_quote(signature),
    };
    lines.insert(end, block.begin(), block.end());
    save_atomic(file, lines);
}

void HistoryService::verify_history_chain(
    const std::filesystem::path& file,
    const std::string& uid) const {
    (void)verify_chain(read_lines(file), uid);
}

void HistoryService::verify_file(
    const std::filesystem::path& file,
    const std::string& uid) const {
    const auto lines = read_lines(file);
    const auto history = verify_chain(lines, uid);
    const auto current = capcom::security::sha256_hex(
        content_without_history(lines));
    if (history.latest_content_hash != current) {
        throw std::runtime_error(
            "UNSIGNED CHANGES: " + uid +
            " was edited after its last valid signature. Run 'cap sign " +
            uid + "'.");
    }
}

void HistoryService::verify_project(const std::filesystem::path& project) const {
    const auto directory = project / "reqs";
    if (!std::filesystem::is_directory(directory)) {
        throw std::runtime_error("Requirements directory not found. Run 'cap init'.");
    }
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file()) continue;
        const auto extension = entry.path().extension().string();
        if (extension != ".yaml" && extension != ".yml") continue;
        std::ifstream input(entry.path());
        std::string line;
        std::string uid;
        while (std::getline(input, line)) {
            if (line.rfind("uid:", 0) == 0) {
                uid = yaml_unquote(line.substr(4));
                break;
            }
        }
        verify_file(entry.path(), uid);
    }
}

} // namespace capcom::audit
