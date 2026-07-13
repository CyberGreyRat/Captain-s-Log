#include "capcom/cli/command_parser.hpp"

#include <iostream>

int main(const int argc, const char* const argv[]) {
    const capcom::cli::CommandParser parser;
    const auto command = parser.parse(argc, argv);

    switch (command.type) {
    case capcom::cli::CommandType::help:
        std::cout << capcom::cli::CommandParser::help_text();
        return 0;
    case capcom::cli::CommandType::version:
        std::cout << "CapCom CLI 0.3.0\n";
        return 0;
    case capcom::cli::CommandType::init:
    case capcom::cli::CommandType::create:
        std::cerr << "Command recognized but not implemented in this scaffold.\n";
        return 2;
    case capcom::cli::CommandType::unknown:
        std::cerr << "Unknown command. Run 'cap help'.\n";
        return 2;
    }

    return 2;
}
