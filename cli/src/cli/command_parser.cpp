#include "capcom/cli/command_parser.hpp"
namespace capcom::cli {
Command CommandParser::parse(const int argc, const char* const argv[]) const {
    if (argc < 2) return {CommandType::help, {}};
    const std::string_view name{argv[1]};
    Command result{};
    if (name == "help" || name == "--help" || name == "-h") result.type = CommandType::help;
    else if (name == "version" || name == "--version" || name == "-v") result.type = CommandType::version;
    else if (name == "init" || name == "-i") result.type = CommandType::init;
    else if (name == "create" || name == "-c") result.type = CommandType::create;
    else if (name == "link" || name == "-l") result.type = CommandType::link;
    else if (name == "unlink" || name == "-u") result.type = CommandType::unlink;
    else if (name == "tree" || name == "-t") result.type = CommandType::tree;
    for (int index = 2; index < argc; ++index) result.arguments.emplace_back(argv[index]);
    return result;
}
std::string_view CommandParser::help_text() noexcept {
    return
        "CapCom CLI 0.3.3\n"
        "Usage:\n"
        "  cap init [directory]                              (-i)\n"
        "  cap create <type> [title] [-p <parent_uid>]       (-c)\n"
        "  cap link <child_uid> <parent_uid>                 (-l)\n"
        "  cap unlink <child_uid> <parent_uid>               (-u)\n"
        "  cap tree                                          (-t)\n"
        "\nExamples:\n"
        "  cap -c usr\n"
        "  cap -c sys -p USR-001\n"
        "  cap -c test \"Format pruefen\" -p SRS-002\n";
}
}
