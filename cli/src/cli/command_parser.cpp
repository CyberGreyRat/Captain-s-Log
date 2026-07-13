#include "capcom/cli/command_parser.hpp"

namespace capcom::cli {

Command CommandParser::parse(const int argc, const char* const argv[]) const {
    // Phase 3 scaffold only. Full validation and subcommand parsing follow
    // after architectural review.
    if (argc < 2) {
        return {CommandType::help, {}};
    }

    const std::string_view name{argv[1]};
    Command command{};

    if (name == "help" || name == "--help" || name == "-h") {
        command.type = CommandType::help;
    } else if (name == "version" || name == "--version") {
        command.type = CommandType::version;
    } else if (name == "init") {
        command.type = CommandType::init;
    } else if (name == "create") {
        command.type = CommandType::create;
    }

    for (int index = 2; index < argc; ++index) {
        command.arguments.emplace_back(argv[index]);
    }
    return command;
}

std::string_view CommandParser::help_text() noexcept {
    return
        "CapCom CLI 0.3.0\n"
        "Usage:\n"
        "  cap init\n"
        "  cap create <type> <title>\n"
        "  cap help\n";
}

} // namespace capcom::cli
