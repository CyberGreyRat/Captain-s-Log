#include "capcom/audit/history_service.hpp"
#include "capcom/security/crypto.hpp"
#include "capcom/yaml/yaml_store.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
namespace capcom::audit
{
    namespace
    {
        std::string trim(std::string s)
        {
            auto a = s.find_first_not_of(" \t\r\n");
            if (a == std::string::npos)
                return {};
            auto b = s.find_last_not_of(" \t\r\n");
            return s.substr(a, b - a + 1);
        }
        std::string unquote(std::string value)
        {
            value = trim(std::move(value));

            if (
                value.size() < 2 ||
                value.front() != '"' ||
                value.back() != '"')
            {
                return value;
            }

            value = value.substr(1, value.size() - 2);

            std::string result;
            result.reserve(value.size());

            bool escaped = false;

            for (const char character : value)
            {
                if (escaped)
                {
                    switch (character)
                    {
                    case 'n':
                        result.push_back('\n');
                        break;

                    case 'r':
                        result.push_back('\r');
                        break;

                    case 't':
                        result.push_back('\t');
                        break;

                    case '\\':
                        result.push_back('\\');
                        break;

                    case '"':
                        result.push_back('"');
                        break;

                    default:
                        result.push_back(character);
                        break;
                    }

                    escaped = false;
                    continue;
                }

                if (character == '\\')
                {
                    escaped = true;
                }
                else
                {
                    result.push_back(character);
                }
            }

            if (escaped)
            {
                result.push_back('\\');
            }

            return result;
        }
        std::string quote(const std::string &s)
        {
            std::string o = "\"";
            for (char c : s)
            {
                if (c == '\\' || c == '"')
                    o += '\\';
                if (c == '\n')
                    o += "\\n";
                else if (c != '\r')
                    o += c;
            }
            return o + '"';
        }
        std::vector<std::string> lines(const std::filesystem::path &p)
        {
            std::ifstream i(p, std::ios::binary);
            if (!i)
                throw std::runtime_error("Cannot read " + p.string());
            std::vector<std::string> v;
            std::string s;
            while (std::getline(i, s))
            {
                if (!s.empty() && s.back() == '\r')
                    s.pop_back();
                v.push_back(s);
            }
            return v;
        }
        void save(const std::filesystem::path &p, const std::vector<std::string> &v)
        {
            std::filesystem::create_directories(p.parent_path());
            auto t = p;
            t += ".tmp";
            {
                std::ofstream o(t, std::ios::binary | std::ios::trunc);
                if (!o)
                    throw std::runtime_error("Cannot write " + t.string());
                for (auto &s : v)
                    o << s << '\n';
            }
            std::error_code e;
            std::filesystem::remove(p, e);
            e.clear();
            std::filesystem::rename(t, p, e);
            if (e)
                throw std::runtime_error("Cannot replace " + p.string() + ": " + e.message());
        }
        std::filesystem::path project_of(const std::filesystem::path &file)
        {
            auto p = std::filesystem::absolute(file).parent_path();
            while (!p.empty())
            {
                if (p.filename() == "reqs")
                    return p.parent_path();
                auto q = p.parent_path();
                if (q == p)
                    break;
                p = q;
            }
            throw std::runtime_error("Requirement is not below reqs/: " + file.string());
        }
        std::filesystem::path audit_file(const std::filesystem::path &file) { return project_of(file) / "reqs" / "captainslog.yml"; }
        std::string utc()
        {
            auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::tm x{};
#ifdef _WIN32
            gmtime_s(&x, &t);
#else
            gmtime_r(&t, &x);
#endif
            std::ostringstream o;
            o << std::put_time(&x, "%Y-%m-%dT%H:%M:%SZ");
            return o.str();
        }
        std::string payload(const AuditEntry &e) { return "CAPTAINS-LOG-HISTORY-V2\nuid=" + e.uid + "\ntimestamp=" + e.timestamp + "\naction=" + e.action + "\nactor=" + e.actor + "\nhost_id=" + e.host_id + "\nkey_id=" + e.key_id + "\ncontent_hash=" + e.content_hash + "\nprevious_signature=" + e.previous_signature + "\nreason=" + e.reason + "\n"; }
        std::vector<AuditEntry> parse_central(const std::filesystem::path &p)
        {
            std::vector<AuditEntry> out;
            if (!std::filesystem::exists(p))
                return out;
            AuditEntry e;
            auto finish = [&]
            {if(!e.uid.empty())out.push_back(e);e={}; };
            for (auto line : lines(p))
            {
                auto x = trim(line);
                if (x.rfind("- uid:", 0) == 0)
                {
                    finish();
                    e.uid = unquote(x.substr(x.find(':') + 1));
                    continue;
                }
                auto c = x.find(':');
                if (c == std::string::npos)
                    continue;
                auto k = trim(x.substr(0, c)), v = unquote(x.substr(c + 1));
                if (k == "timestamp")
                    e.timestamp = v;
                else if (k == "action")
                    e.action = v;
                else if (k == "actor")
                    e.actor = v;
                else if (k == "host_id")
                    e.host_id = v;
                else if (k == "key_id")
                    e.key_id = v;
                else if (k == "algorithm")
                    e.algorithm = v;
                else if (k == "public_key")
                    e.public_key = v;
                else if (k == "content_hash")
                    e.content_hash = v;
                else if (k == "previous_signature")
                    e.previous_signature = v;
                else if (k == "reason")
                    e.reason = v;
                else if (k == "signature")
                    e.signature = v;
            }
            finish();
            return out;
        }
        std::vector<std::string> render(const std::vector<AuditEntry> &entries)
        {
            std::vector<std::string> v{"schema_version: 2", "entries:"};
            for (auto &e : entries)
            {
                v.push_back("  - uid: " + quote(e.uid));
                v.push_back("    timestamp: " + quote(e.timestamp));
                v.push_back("    action: " + quote(e.action));
                v.push_back("    actor: " + quote(e.actor));
                v.push_back("    host_id: " + quote(e.host_id));
                v.push_back("    key_id: " + quote(e.key_id));
                v.push_back("    algorithm: " + quote(e.algorithm));
                v.push_back("    public_key: " + quote(e.public_key));
                v.push_back("    content_hash: " + quote(e.content_hash));
                v.push_back("    previous_signature: " + quote(e.previous_signature));
                v.push_back("    reason: " + quote(e.reason));
                v.push_back("    signature: " + quote(e.signature));
            }
            return v;
        }
        std::vector<AuditEntry> for_uid(const std::filesystem::path &file, const std::string &uid)
        {
            auto all = parse_central(audit_file(file));
            std::vector<AuditEntry> out;
            for (auto &e : all)
                if (e.uid == uid)
                    out.push_back(e);
            return out;
        }
        void verify_entries(const std::vector<AuditEntry> &es, const std::string &uid)
        {
            if (es.empty())
                throw std::runtime_error("SECURITY ALERT: Unsigned history in requirement " + uid);
            std::string previous = "GENESIS";
            for (auto &e : es)
            {
                if (e.previous_signature != previous || !capcom::security::verify_rsa_pss_sha256(e.public_key, payload(e), e.signature))
                    throw std::runtime_error("SECURITY ALERT: Invalid central audit chain for " + uid);
                previous = e.signature;
            }
        }
        std::vector<AuditEntry> embedded(const std::filesystem::path &file, const std::string &uid)
        {
            std::vector<AuditEntry> out;
            AuditEntry e;
            e.uid = uid;
            bool in = false;
            auto finish = [&]
            {if(!e.timestamp.empty())out.push_back(e);e={};e.uid=uid; };
            for (auto line : lines(file))
            {
                if (line.rfind("history:", 0) == 0)
                {
                    in = true;
                    continue;
                }
                if (!in)
                    continue;
                if (!line.empty() && line.front() != ' ' && line.front() != '\t')
                    break;
                auto x = trim(line);
                if (x.rfind("- ", 0) == 0)
                    x = trim(x.substr(2));
                auto c = x.find(':');
                if (c == std::string::npos)
                    continue;
                auto k = trim(x.substr(0, c)), v = unquote(x.substr(c + 1));
                if (k == "timestamp")
                {
                    finish();
                    e.timestamp = v;
                }
                else if (k == "action")
                    e.action = v;
                else if (k == "actor")
                    e.actor = v;
                else if (k == "host_id")
                    e.host_id = v;
                else if (k == "key_id")
                    e.key_id = v;
                else if (k == "algorithm")
                    e.algorithm = v;
                else if (k == "public_key")
                    e.public_key = v;
                else if (k == "content_hash")
                    e.content_hash = v;
                else if (k == "previous_signature")
                    e.previous_signature = v;
                else if (k == "reason")
                    e.reason = v;
                else if (k == "signature")
                    e.signature = v;
            }
            finish();
            return out;
        }
        void strip_embedded(const std::filesystem::path &file)
        {
            auto v = lines(file);
            std::vector<std::string> o;
            bool in = false;
            for (auto &s : v)
            {
                if (s.rfind("history:", 0) == 0)
                {
                    in = true;
                    continue;
                }
                if (in && !s.empty() && s.front() != ' ' && s.front() != '\t')
                    in = false;
                if (!in)
                    o.push_back(s);
            }
            save(file, o);
        }
    }
    HistoryService::HistoryService(capcom::identity::Identity i) : identity_(std::move(i)) {}
    std::string HistoryService::current_content_hash(const std::filesystem::path &f) const
    {
        std::ifstream i(f, std::ios::binary);
        return capcom::security::sha256_hex(std::string{std::istreambuf_iterator<char>{i}, {}});
    }
    std::vector<AuditEntry> HistoryService::entries(const std::filesystem::path &f, const std::string &uid) const { return for_uid(f, uid); }
    std::string HistoryService::latest_signed_content_hash(const std::filesystem::path &f) const
    {
        auto es = for_uid(f, capcom::yaml::normalize_uid(f.stem().string()));
        return es.empty() ? "" : es.back().content_hash;
    }
    void HistoryService::verify_history_chain(const std::filesystem::path &f, const std::string &uid) const { verify_entries(for_uid(f, uid), uid); }
    bool HistoryService::has_unsigned_content_changes(const std::filesystem::path &f, const std::string &uid) const
    {
        verify_history_chain(f, uid);
        auto es = for_uid(f, uid);
        return current_content_hash(f) != es.back().content_hash;
    }
    void HistoryService::verify_file(const std::filesystem::path &f, const std::string &uid) const
    {
        auto es = for_uid(f, uid);
        verify_entries(es, uid);
        if (current_content_hash(f) != es.back().content_hash)
            throw std::runtime_error("UNSIGNED CHANGES: " + uid + " was edited after its last valid signature. Run 'cap sign " + uid + "'.");
    }
    void HistoryService::append(const std::filesystem::path &f, const std::string &uid, const std::string &action, const std::string &reason) const
    {
        auto audit = audit_file(f);
        auto all = parse_central(audit);
        auto own = for_uid(f, uid);
        if (!own.empty())
            verify_entries(own, uid);
        AuditEntry e{uid, utc(), action, identity_.full_name, identity_.host_id, identity_.key_id, "RSA-PSS-SHA256", identity_.public_key_base64, current_content_hash(f), own.empty() ? "GENESIS" : own.back().signature, reason, {}};
        e.signature = capcom::security::sign_rsa_pss_sha256(identity_.key_name, payload(e));
        all.push_back(e);
        save(audit, render(all));
    }
    void HistoryService::verify_project(const std::filesystem::path &project) const
    {
        for (auto &[uid, item] : capcom::yaml::YamlStore{project}.load_all())
            verify_file(item.file, uid);
    }
    std::string HistoryService::first_key_for_action(const std::filesystem::path &f, const std::string &uid, const std::string &a) const
    {
        for (auto &e : for_uid(f, uid))
            if (e.action == a)
                return e.key_id;
        return {};
    }
    std::string HistoryService::latest_key_for_action(const std::filesystem::path &f, const std::string &uid, const std::string &a) const
    {
        std::string k;
        for (auto &e : for_uid(f, uid))
            if (e.action == a)
                k = e.key_id;
        return k;
    }
    void HistoryService::migrate_project(const std::filesystem::path &project) const
    {
        auto audit = project / "reqs" / "captainslog.yml";
        auto all = parse_central(audit);
        for (auto &[uid, item] : capcom::yaml::YamlStore{project}.load_all())
        {
            auto old = embedded(item.file, uid);
            if (!old.empty())
            {
                verify_entries(old, uid);
                bool exists = false;
                for (const auto &entry : all)
                    if (entry.uid == uid)
                    {
                        exists = true;
                        break;
                    }
                if (!exists)
                    all.insert(all.end(), old.begin(), old.end());
                strip_embedded(item.file);
            }
        }
        save(audit, render(all));
    }
}
