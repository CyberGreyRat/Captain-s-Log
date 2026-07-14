#include "capcom/audit/history_service.hpp"
#include "capcom/security/crypto.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <stdexcept>
namespace capcom::audit
{
    namespace
    {
        std::string timestamp()
        {
            const auto time = std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::now());

            std::tm utc_time{};

#ifdef _WIN32
            gmtime_s(&utc_time, &time);
#else
            gmtime_r(&time, &utc_time);
#endif

            std::ostringstream output;

            output
                << std::put_time(
                       &utc_time,
                       "%Y-%m-%dT%H:%M:%SZ");

            return output.str();
        }
        std::string quote(std::string value)
        {
            std::string out{"\""};
            for (char c : value)
            {
                if (c == '\\' || c == '"')
                    out.push_back('\\');
                out.push_back(c);
            }
            return out + '"';
        }
        std::vector<std::string> read_lines(const std::filesystem::path &file)
        {
            std::ifstream in(file);
            if (!in)
                throw std::runtime_error("Cannot read " + file.string());
            std::vector<std::string> result;
            std::string line;
            while (std::getline(in, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                result.push_back(line);
            }
            return result;
        }
        void save(const std::filesystem::path &file, const std::vector<std::string> &lines)
        {
            auto temp = file;
            temp += ".audit.tmp";
            {
                std::ofstream out(temp, std::ios::trunc);
                for (const auto &line : lines)
                    out << line << '\n';
            }
            std::error_code error;
            std::filesystem::remove(file, error);
            error.clear();
            std::filesystem::rename(temp, file, error);
            if (error)
                throw std::runtime_error("Cannot update audit history: " + error.message());
        }
        std::string content_without_history(const std::vector<std::string> &lines)
        {
            std::ostringstream out;
            bool skip = false;
            for (const auto &line : lines)
            {
                if (line.rfind("history:", 0) == 0)
                {
                    skip = true;
                    continue;
                }
                if (skip && !line.empty() && line.front() != ' ' && line.front() != '\t')
                    skip = false;
                if (!skip)
                    out << line << '\n';
            }
            return out.str();
        }
        std::string scalar(const std::string &line)
        {
            const auto colon = line.find(':');
            if (colon == std::string::npos)
                return {};
            auto value = line.substr(colon + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
                value = value.substr(1, value.size() - 2);
            return value;
        }
        std::string payload(const std::string &uid, const std::string &time,
                            const std::string &action, const std::string &actor,
                            const std::string &host, const std::string &key,
                            const std::string &hash, const std::string &previous)
        {
            return "CAPTAINS-LOG-HISTORY-V1\nuid=" + uid + "\ntimestamp=" + time +
                   "\naction=" + action + "\nactor=" + actor + "\nhost_id=" + host +
                   "\nkey_id=" + key + "\ncontent_hash=" + hash +
                   "\nprevious_signature=" + previous + "\n";
        }
    }
    HistoryService::HistoryService(capcom::identity::Identity identity)
        : identity_{std::move(identity)} {}
    void HistoryService::append(const std::filesystem::path &file,
                                const std::string &uid,
                                const std::string &action) const
    {
        auto lines = read_lines(file);
        std::string previous = "GENESIS";
        for (const auto &line : lines)
        {
            if (line.find("signature:") != std::string::npos)
                previous = scalar(line);
        }
        const auto time = timestamp();
        const auto hash = capcom::security::sha256_hex(content_without_history(lines));
        const auto canonical = payload(uid, time, action, identity_.full_name,
                                       identity_.host_id, identity_.key_id, hash, previous);
        const auto signature = capcom::security::sign_rsa_pss_sha256(
            identity_.key_name, canonical);
        auto position = std::find_if(lines.begin(), lines.end(), [](const std::string &line)
                                     { return line.rfind("history:", 0) == 0; });
        if (position == lines.end())
        {
            auto insertion = std::find_if(lines.begin(), lines.end(), [](const std::string &line)
                                          { return line.rfind("integration_template:", 0) == 0; });
            position = lines.insert(insertion, "history:");
        }
        auto end = position + 1;
        while (end != lines.end() && (end->empty() || end->front() == ' ' || end->front() == '\t'))
            ++end;
        const std::vector<std::string> block{
            "  - timestamp: " + quote(time), "    action: " + quote(action),
            "    actor: " + quote(identity_.full_name), "    host_id: " + quote(identity_.host_id),
            "    key_id: " + quote(identity_.key_id), "    algorithm: \"RSA-PSS-SHA256\"",
            "    public_key: " + quote(identity_.public_key_base64),
            "    content_hash: " + quote(hash), "    previous_signature: " + quote(previous),
            "    signature: " + quote(signature)};
        lines.insert(end, block.begin(), block.end());
        save(file, lines);
    }
    void HistoryService::verify_file(const std::filesystem::path &file,
                                     const std::string &uid) const
    {
        const auto lines = read_lines(file);
        const auto current_hash = capcom::security::sha256_hex(content_without_history(lines));
        std::string previous = "GENESIS";
        std::string latest_content_hash;
        std::map<std::string, std::string> entry;
        int count = 0;
        auto verify_entry = [&]()
        {
            if (entry.empty())
                return;
            ++count;
            const auto canonical = payload(uid, entry["timestamp"], entry["action"],
                                           entry["actor"], entry["host_id"], entry["key_id"],
                                           entry["content_hash"], entry["previous_signature"]);
            if (entry["previous_signature"] != previous ||
                !capcom::security::verify_rsa_pss_sha256(entry["public_key"],
                                                         canonical, entry["signature"]))
            {
                throw std::runtime_error("SECURITY ALERT: Tampering detected in requirement " + uid + "! History entry signature invalid.");
            }
            previous = entry["signature"];
            latest_content_hash = entry["content_hash"];
            entry.clear();
        };
        bool history = false;
        for (const auto &line : lines)
        {
            if (line.rfind("history:", 0) == 0)
            {
                history = true;
                continue;
            }
            if (!history)
                continue;
            if (!line.empty() && line.front() != ' ' && line.front() != '\t')
                break;
            auto trimmed = line;
            trimmed.erase(0, trimmed.find_first_not_of(" \t-"));
            const auto colon = trimmed.find(':');
            if (colon == std::string::npos)
                continue;
            const auto key = trimmed.substr(0, colon);
            if (key == "timestamp" && !entry.empty())
                verify_entry();
            entry[key] = scalar(trimmed);
        }
        verify_entry();
        if (count == 0)
            throw std::runtime_error("SECURITY ALERT: Unsigned history in requirement " + uid);
        if (latest_content_hash != current_hash)
        {
            throw std::runtime_error("SECURITY ALERT: Tampering detected in requirement " + uid + "! YAML content hash changed.");
        }
    }
    void HistoryService::verify_project(const std::filesystem::path &project) const
    {
        for (const auto &entry : std::filesystem::directory_iterator(project / "reqs"))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".yaml")
                continue;
            std::ifstream in(entry.path());
            std::string line, uid;
            while (std::getline(in, line))
                if (line.rfind("uid:", 0) == 0)
                {
                    uid = scalar(line);
                    break;
                }
            verify_file(entry.path(), uid);
        }
    }
}
