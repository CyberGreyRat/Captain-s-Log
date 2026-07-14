#include "capcom/commands/yaml_commands.hpp"
#include "capcom/audit/history_service.hpp"
#include "capcom/identity/identity_manager.hpp"
#include "capcom/yaml/yaml_store.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
namespace capcom::commands
{
    namespace
    {
        bool source(const std::filesystem::path &p)
        {
            auto e = p.extension().string();
            return e == ".c" || e == ".cpp" || e == ".h" || e == ".hpp" || e == ".cc" || e == ".cxx";
        }
        bool ignored(const std::filesystem::path &p)
        {
            auto s = p.generic_string();
            return s.find("/.git/") != std::string::npos || s.find("/build/") != std::string::npos || s.find("/.venv/") != std::string::npos || s.find("/distribution/") != std::string::npos;
        }
        std::string git_hash(const std::filesystem::path &p)
        {
            auto f = p / ".cap_git_hash.tmp";
            auto c = "git -C \"" + p.string() + "\" rev-parse --short HEAD > \"" + f.string() + "\" 2>nul";
            std::system(c.c_str());
            std::ifstream i(f);
            std::string h;
            std::getline(i, h);
            std::error_code e;
            std::filesystem::remove(f, e);
            return h.empty() ? "UNKNOWN" : h;
        }
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
        std::string rel(const std::filesystem::path &f, const std::filesystem::path &p)
        {
            std::error_code e;
            auto r = std::filesystem::relative(f, p, e);
            return e ? f.generic_string() : r.generic_string();
        }
        std::string quote_cmd(const std::string &value)
        {
            if (
                value.find(' ') == std::string::npos &&
                value.find('\t') == std::string::npos &&
                value.find('"') == std::string::npos)
            {
                return value;
            }

            std::string result{"\""};

            for (const char character : value)
            {
                if (character == '"')
                {
                    result += "\\\"";
                }
                else
                {
                    result.push_back(character);
                }
            }

            result.push_back('"');
            return result;
        }
    }
    int ScanCommand::execute(const std::filesystem::path &p) const
    {
        capcom::yaml::YamlStore store{p};
        auto items = store.load_all();
        auto identity = capcom::identity::IdentityManager{}.load_or_create();
        capcom::audit::HistoryService history{identity};
        std::map<std::string, std::string> before_hash;
        for (auto &[uid, item] : items)
        {
            history.verify_file(item.file, uid);
            before_hash[uid] = history.current_content_hash(item.file);
        }
        std::map<std::string, std::vector<capcom::yaml::Implementation>> found;
        std::regex start(R"CAP(/\*\s*@cap-start:\s*([A-Za-z0-9_-]+))CAP"), end(R"CAP(/\*\s*@cap-end:\s*([A-Za-z0-9_-]+)\s*\*/)CAP");
        auto hash = git_hash(p);
        for (auto it = std::filesystem::recursive_directory_iterator(p); it != std::filesystem::recursive_directory_iterator(); ++it)
        {
            if (it->is_directory() && ignored(it->path()))
            {
                it.disable_recursion_pending();
                continue;
            }
            if (!it->is_regular_file() || !source(it->path()) || ignored(it->path()))
                continue;
            std::ifstream in(it->path());
            std::string line, active;
            int n = 0, begin = 0;
            while (std::getline(in, line))
            {
                ++n;
                std::smatch m;
                if (std::regex_search(line, m, start))
                {
                    if (!active.empty())
                        throw std::runtime_error("Nested @cap-start in " + it->path().string());
                    active = capcom::yaml::normalize_uid(m[1].str());
                    begin = n;
                }
                if (std::regex_search(line, m, end))
                {
                    auto uid = capcom::yaml::normalize_uid(m[1].str());
                    if (active.empty() || uid != active)
                        throw std::runtime_error("Mismatched @cap-end in " + it->path().string());
                    if (!items.contains(uid))
                        throw std::runtime_error("Unknown UID in source: " + uid);
                    found[uid].push_back({rel(it->path(), p), begin, n, hash});
                    active.clear();
                }
            }
            if (!active.empty())
                throw std::runtime_error("Missing @cap-end for " + active);
        }
        store.replace_implementations(found);
        auto after = store.load_all();
        for (auto &[uid, item] : after)
            if (before_hash[uid] != history.current_content_hash(item.file))
                history.append(item.file, uid, "IMPLEMENTATION_SCANNED", "Source markers scanned at Git " + hash);
        std::cout << "Scan complete: " << found.size() << " item(s) implemented. Git " << hash << '\n';
        for (const auto &[uid, implementations] : found)
        {
            std::cout << "  " << uid << '\n';
            for (const auto &implementation : implementations)
            {
                std::cout << "    " << implementation.file << ':' << implementation.start_line << '-' << implementation.end_line << '\n';
            }
        }
        return 0;
    }
    int StatusCommand::execute(const std::filesystem::path &p) const
    {
        auto m = capcom::yaml::YamlStore{p}.load_all();
        std::cout << std::left << std::setw(10) << "UID" << std::setw(34) << "TITLE" << std::setw(14) << "STATUS" << std::setw(10) << "IMPL" << "TEST\n"
                  << std::string(78, '-') << '\n';
        for (auto &[uid, i] : m)
            std::cout << std::setw(10) << uid << std::setw(34) << i.title.substr(0, 32) << std::setw(14) << i.status << std::setw(10) << (!i.implementations.empty() ? "Ja" : "Nein") << ((i.test.status == "Passed" || i.test.status == "Tested") ? "Ja" : "Nein") << '\n';
        return 0;
    }
    int ValidateCommand::execute(const std::filesystem::path &p) const
    {
        capcom::yaml::YamlStore s{p};
        auto e = s.validate(s.load_all());
        if (e.empty())
        {
            std::cout << "Validation successful.\n";
            return 0;
        }
        std::cerr << "Validation failed:\n";
        for (auto &x : e)
            std::cerr << "  - " << x << '\n';
        return 2;
    }
    int TestImportCommand::execute(const std::filesystem::path &p, const std::filesystem::path &f) const
    {
        std::ifstream in(f, std::ios::binary);
        if (!in)
            throw std::runtime_error("Cannot read test result: " + f.string());
        std::string data{std::istreambuf_iterator<char>{in}, {}};
        capcom::yaml::YamlStore store{p};
        std::vector<std::pair<std::string, capcom::yaml::TestResult>> results;
        auto src = rel(f, p);
        if (f.extension() == ".json")
        {
            std::regex pat(R"CAP("uid"\s*:\s*"([^"]+)"[\s\S]*?"status"\s*:\s*"([^"]+)"(?:[\s\S]*?"message"\s*:\s*"([^"]*)")?)CAP");
            for (auto i = std::sregex_iterator(data.begin(), data.end(), pat); i != std::sregex_iterator(); ++i)
                results.push_back({capcom::yaml::normalize_uid((*i)[1].str()), {(*i)[2].str(), src, utc(), (*i)[3].str()}});
        }
        else if (f.extension() == ".xml")
        {
            std::regex tc(R"CAP(<testcase[^>]*name="([^"]+)"[^>]*(?:>([\s\S]*?)</testcase>|/>))CAP"), uidp(R"CAP(([A-Za-z]+)[_-]?(\d+))CAP");
            for (auto i = std::sregex_iterator(data.begin(), data.end(), tc); i != std::sregex_iterator(); ++i)
            {
                std::smatch u;
                const auto test_name = (*i)[1].str();
                if (!std::regex_search(test_name, u, uidp))
                    continue;
                auto body = (*i)[2].str();
                bool fail = body.find("<failure") != std::string::npos || body.find("<error") != std::string::npos;
                results.push_back({capcom::yaml::normalize_uid(u[1].str()), {fail ? "Failed" : "Passed", src, utc(), fail ? "See JUnit report" : ""}});
            }
        }
        else
            throw std::runtime_error("Unsupported result format. Use .xml or .json.");
        if (results.empty())
        {
            throw std::runtime_error("No Captain's Log test IDs found.");
        }

        const auto identity =
            capcom::identity::IdentityManager{}.load_or_create();
        capcom::audit::HistoryService history{identity};

        for (auto &[uid, result] : results)
        {
            const auto current = store.load_all();
            const auto found = current.find(uid);
            if (found == current.end())
            {
                throw std::runtime_error("Test UID not found: " + uid);
            }

            const auto &type = found->second.type;
            if (!(type == "UT" || type == "IT" || type == "ST" ||
                  type == "AT" || type == "TEST"))
            {
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
    int TestRunCommand::execute(const std::filesystem::path &p, const std::string &raw, const std::vector<std::string> &args) const
    {
        if (args.empty())
            throw std::runtime_error("Usage: cap test run <UID> -- <program> [args]");
        auto uid = capcom::yaml::normalize_uid(raw);
        auto items = capcom::yaml::YamlStore{p}.load_all();
        auto it = items.find(uid);
        if (it == items.end())
            throw std::runtime_error("Test UID not found: " + uid);
        if (!(it->second.type == "UT" || it->second.type == "IT" || it->second.type == "ST" || it->second.type == "AT" || it->second.type == "TEST"))
            throw std::runtime_error("cap test run requires a test item UID.");
        auto out = p / "reports" / (uid + "-run.txt");
        std::filesystem::create_directories(out.parent_path());
        std::string cmd;
        for (auto &a : args)
        {
            if (!cmd.empty())
                cmd += ' ';
            cmd += quote_cmd(a);
        }
        cmd += " > " + quote_cmd(out.string()) + " 2>&1";
        int code = std::system(cmd.c_str());
        std::ifstream input(out);
        std::string message{std::istreambuf_iterator<char>{input}, {}};
        if (message.size() > 500)
            message.resize(500);
        capcom::yaml::TestResult r{code == 0 ? "Passed" : "Failed", rel(out, p), utc(), message};
        capcom::yaml::YamlStore store{p};
        store.update_test(uid, r);
        auto item = store.load_all().at(uid);
        auto identity = capcom::identity::IdentityManager{}.load_or_create();
        capcom::audit::HistoryService h{identity};
        h.append(item.file, uid, code == 0 ? "TEST_PASSED" : "TEST_FAILED", "Executed: " + cmd);
        std::cout << uid << " " << r.status << " (exit " << code << ")\n";
        return code == 0 ? 0 : 2;
    }
}
