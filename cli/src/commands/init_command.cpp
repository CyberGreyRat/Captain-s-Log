#include "capcom/commands/init_command.hpp"
#include "capcom/config/app_config.hpp"

#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace capcom::commands {

int InitCommand::execute(const std::filesystem::path& target) const {
    std::filesystem::create_directories(target);

    constexpr std::array root_directories{
        "src", "include", "tests", "reports", "hardware"
    };
    for (const auto* directory : root_directories) {
        std::filesystem::create_directories(target / directory);
    }

    constexpr std::array requirement_types{
        "usr", "sys", "srs", "sec", "arch", "lld", "risk", "class",
        "test", "ut", "it", "st", "at", "bug", "cr", "soup"
    };
    for (const auto* type : requirement_types) {
        std::filesystem::create_directories(target / "reqs" / type);
    }

    const auto audit_file = target / "reqs" / "captainslog.yml";
    if (!std::filesystem::exists(audit_file)) {
        std::ofstream output(audit_file, std::ios::binary);
        if (!output) {
            throw std::runtime_error("Cannot create " + audit_file.string());
        }
        output << "schema_version: 2\nentries:\n";
    }

    capcom::config::ConfigLoader{}.write_default(target);

    const auto gitignore = target / ".gitignore";
    if (!std::filesystem::exists(gitignore)) {
        std::ofstream output(gitignore, std::ios::binary);
        if (!output) {
            throw std::runtime_error("Cannot create " + gitignore.string());
        }
        output << "reports/\n*.exe\n.cap_git_hash.tmp\n";
    }

    std::cout << "Initialized Captain's Log project: "
              << std::filesystem::absolute(target).string() << '\n';
    return 0;
}

} // namespace capcom::commands
