#include "capcom/cli/command_parser.hpp"
#include "capcom/commands/create_command.hpp"
#include "capcom/commands/init_command.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
int main(const int argc, const char* const argv[]) {
    try {
        const auto command = capcom::cli::CommandParser{}.parse(argc, argv);
        switch (command.type) {
        case capcom::cli::CommandType::help: std::cout << capcom::cli::CommandParser::help_text(); return 0;
        case capcom::cli::CommandType::version: std::cout << "CapCom CLI 0.3.1\n"; return 0;
        case capcom::cli::CommandType::init: {
            if (command.arguments.size() > 1) throw std::runtime_error("Usage: cap init [directory]");
            const std::filesystem::path target = command.arguments.empty()
                ? std::filesystem::current_path()
                : std::filesystem::path{command.arguments.front()};
            return capcom::commands::InitCommand{}.execute(target);
        }
        case capcom::cli::CommandType::create:
            if (command.arguments.size() != 2) throw std::runtime_error("Usage: cap create <type> <title>");
            return capcom::commands::CreateCommand{}.execute(std::filesystem::current_path(), command.arguments[0], command.arguments[1]);
        case capcom::cli::CommandType::unknown: throw std::runtime_error("Unknown command. Run 'cap help'.");
        }
    } catch (const std::exception& error) { std::cerr << "Error: " << error.what() << '\n'; return 2; }
    return 2;
}
