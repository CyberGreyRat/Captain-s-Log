#include "capcom/commands/init_command.hpp"
#include "capcom/config/app_config.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace capcom::commands {
int InitCommand::execute(const std::filesystem::path& target) const {
    std::filesystem::create_directories(target);
    const std::array dirs{"reqs", "src", "include", "tests", "reports", "hardware"};
    for (const auto* dir : dirs) std::filesystem::create_directories(target / dir);
    capcom::config::ConfigLoader{}.write_default(target);
    const auto gitignore = target / ".gitignore";
    if (!std::filesystem::exists(gitignore)) {
        std::ofstream out(gitignore, std::ios::binary);
        if (!out) throw std::runtime_error("Cannot create " + gitignore.string());
        out << "backend/db/traceability.db-wal\nbackend/db/traceability.db-shm\nreports/\n";
    }
    std::cout << "Initialized CapCom project: " << std::filesystem::absolute(target).string() << '\n';
    return 0;
}
}
