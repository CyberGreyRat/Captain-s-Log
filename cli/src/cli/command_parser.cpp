#include "capcom/cli/command_parser.hpp"
namespace capcom::cli {
Command CommandParser::parse(const int argc, const char* const argv[]) const {
    if (argc < 2) return {CommandType::help, {}};
    const std::string_view name{argv[1]};
    Command result{};
    if (name == "help" || name == "--help" || name == "-h") result.type = CommandType::help;
    else if (name == "version" || name == "--version") result.type = CommandType::version;
    else if (name == "init") result.type = CommandType::init;
    else if (name == "create") result.type = CommandType::create;
    for (int i = 2; i < argc; ++i) result.arguments.emplace_back(argv[i]);
    return result;
}
std::string_view CommandParser::help_text() noexcept {
    return "CapCom CLI 0.3.1\nUsage:\n  cap init [directory]\n  cap create <type> <title>\n  cap help\n";
}
}
