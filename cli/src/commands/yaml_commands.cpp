#include "capcom/commands/yaml_commands.hpp"
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
#include <string>

namespace capcom::commands {
namespace {

bool is_source_file(const std::filesystem::path& path) {
    const auto extension = path.extension().string();
    return extension == ".c" || extension == ".cpp" || extension == ".h" ||
           extension == ".hpp" || extension == ".cc" || extension == ".cxx";
}

bool is_ignored_path(const std::filesystem::path& path) {
    const auto value = path.generic_string();
    return value.find("/.git/") != std::string::npos ||
           value.find("/build/") != std::string::npos ||
           value.find("/.venv/") != std::string::npos ||
           value.find("/distribution/") != std::string::npos;
}

std::string read_git_hash(const std::filesystem::path& project) {
    const auto output_file = project / ".cap_git_hash.tmp";
    const auto command =
        "git -C \"" + project.string() +
        "\" rev-parse --short HEAD > \"" + output_file.string() +
        "\" 2>nul";

    std::system(command.c_str());

    std::ifstream input(output_file);
    std::string hash;
    std::getline(input, hash);

    std::error_code ignored;
    std::filesystem::remove(output_file, ignored);

    return hash.empty() ? "UNKNOWN" : hash;
}

std::string current_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time{};

#ifdef _WIN32
    localtime_s(&local_time, &time);
#else
    localtime_r(&time, &local_time);
#endif

    std::ostringstream output;
    output << std::put_time(&local_time, "%Y-%m-%dT%H:%M:%S");
    return output.str();
}

std::string relative_or_original(
    const std::filesystem::path& file,
    const std::filesystem::path& project) {

    std::error_code error;
    const auto relative = std::filesystem::relative(file, project, error);
    return error ? file.generic_string() : relative.generic_string();
}

} // namespace

int ScanCommand::execute(const std::filesystem::path& project) const {
    capcom::yaml::YamlStore store{project};
    const auto items = store.load_all();

    std::map<std::string, std::vector<capcom::yaml::Implementation>> found;

    const std::regex start_pattern(
        R"CAP(/\*\s*@cap-start:\s*([A-Za-z0-9_-]+)(?:\s*\*/)?\s*)CAP");
    const std::regex end_pattern(
        R"CAP(/\*\s*@cap-end:\s*([A-Za-z0-9_-]+)\s*\*/)CAP");

    const auto git_hash = read_git_hash(project);

    for (auto iterator = std::filesystem::recursive_directory_iterator(project);
         iterator != std::filesystem::recursive_directory_iterator();
         ++iterator) {

        if (iterator->is_directory() && is_ignored_path(iterator->path())) {
            iterator.disable_recursion_pending();
            continue;
        }

        if (!iterator->is_regular_file() ||
            !is_source_file(iterator->path()) ||
            is_ignored_path(iterator->path())) {
            continue;
        }

        std::ifstream input(iterator->path());
        if (!input) {
            throw std::runtime_error(
                "Cannot read source file: " + iterator->path().string());
        }

        std::string line;
        std::string active_uid;
        int line_number = 0;
        int start_line = 0;

        while (std::getline(input, line)) {
            ++line_number;
            std::smatch match;

            if (std::regex_search(line, match, start_pattern)) {
                if (!active_uid.empty()) {
                    throw std::runtime_error(
                        "Nested @cap-start in " + iterator->path().string());
                }

                active_uid = capcom::yaml::normalize_uid(match[1].str());
                start_line = line_number;
            }

            if (std::regex_search(line, match, end_pattern)) {
                const auto end_uid =
                    capcom::yaml::normalize_uid(match[1].str());

                if (active_uid.empty() || end_uid != active_uid) {
                    throw std::runtime_error(
                        "Mismatched @cap-end in " + iterator->path().string());
                }

                if (!items.contains(end_uid)) {
                    throw std::runtime_error(
                        "Unknown UID in source: " + end_uid);
                }

                found[end_uid].push_back({
                    relative_or_original(iterator->path(), project),
                    start_line,
                    line_number,
                    git_hash,
                });

                active_uid.clear();
            }
        }

        if (!active_uid.empty()) {
            throw std::runtime_error(
                "Missing @cap-end for " + active_uid + " in " +
                iterator->path().string());
        }
    }

    store.replace_implementations(found);
    std::cout << "Scan complete: " << found.size()
              << " item(s) implemented. Git " << git_hash << '\n';
    return 0;
}

int StatusCommand::execute(const std::filesystem::path& project) const {
    const auto items = capcom::yaml::YamlStore{project}.load_all();

    std::cout << std::left << std::setw(10) << "UID"
              << std::setw(34) << "TITLE"
              << std::setw(14) << "STATUS"
              << std::setw(10) << "IMPL"
              << "TEST\n"
              << std::string(78, '-') << '\n';

    for (const auto& [uid, item] : items) {
        const bool implemented = !item.implementations.empty();
        const bool tested =
            item.test.status == "Passed" || item.test.status == "Tested";
        const bool warning = item.status == "Draft" || !implemented;

        std::cout << (warning ? "\x1b[33m" : "\x1b[32m")
                  << std::setw(10) << uid
                  << std::setw(34) << item.title.substr(0, 32)
                  << std::setw(14) << item.status
                  << std::setw(10) << (implemented ? "Ja" : "Nein")
                  << (tested ? "Ja" : "Nein")
                  << "\x1b[0m\n";
    }

    return 0;
}

int ValidateCommand::execute(const std::filesystem::path& project) const {
    capcom::yaml::YamlStore store{project};
    const auto errors = store.validate(store.load_all());

    if (errors.empty()) {
        std::cout << "Validation successful.\n";
        return 0;
    }

    std::cerr << "Validation failed:\n";
    for (const auto& error : errors) {
        std::cerr << "  - " << error << '\n';
    }
    return 2;
}

int TestImportCommand::execute(
    const std::filesystem::path& project,
    const std::filesystem::path& result_file) const {

    std::ifstream input(result_file, std::ios::binary);
    if (!input) {
        throw std::runtime_error(
            "Cannot read test result: " + result_file.string());
    }

    const std::string data{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{},
    };

    capcom::yaml::YamlStore store{project};
    int imported_count = 0;
    const auto source = relative_or_original(result_file, project);

    if (result_file.extension() == ".xml") {
        const std::regex testcase_pattern(
            R"CAP(<testcase[^>]*name="([^"]+)"[^>]*>([\s\S]*?)</testcase>|<testcase[^>]*name="([^"]+)"[^>]*/>)CAP");
        const std::regex uid_pattern(
            R"CAP(([A-Za-z]+)[_-]?(\d+))CAP");

        for (auto iterator = std::sregex_iterator(
                 data.begin(), data.end(), testcase_pattern);
             iterator != std::sregex_iterator();
             ++iterator) {

            const auto name = (*iterator)[1].matched
                ? (*iterator)[1].str()
                : (*iterator)[3].str();

            std::smatch uid_match;
            if (!std::regex_search(name, uid_match, uid_pattern)) {
                continue;
            }

            const auto uid = capcom::yaml::normalize_uid(
                uid_match[1].str() + "-" + uid_match[2].str());
            const auto body = (*iterator)[2].str();
            const bool failed =
                body.find("<failure") != std::string::npos ||
                body.find("<error") != std::string::npos;

            store.update_test(uid, {
                failed ? "Failed" : "Passed",
                source,
                current_timestamp(),
                failed ? "See JUnit report" : "",
            });
            ++imported_count;
        }
    } else if (result_file.extension() == ".json") {
        const std::regex result_pattern(
            R"CAP("uid"\s*:\s*"([^"]+)"[\s\S]*?"status"\s*:\s*"([^"]+)"(?:[\s\S]*?"message"\s*:\s*"([^"]*)")?)CAP");

        for (auto iterator = std::sregex_iterator(
                 data.begin(), data.end(), result_pattern);
             iterator != std::sregex_iterator();
             ++iterator) {

            store.update_test((*iterator)[1].str(), {
                (*iterator)[2].str(),
                source,
                current_timestamp(),
                (*iterator)[3].str(),
            });
            ++imported_count;
        }
    } else {
        throw std::runtime_error(
            "Unsupported result format. Use .xml or .json.");
    }

    if (imported_count == 0) {
        throw std::runtime_error(
            "No CapCom test result IDs were found in " +
            result_file.string());
    }

    std::cout << "Imported " << imported_count << " test result(s).\n";
    return 0;
}

} // namespace capcom::commands
