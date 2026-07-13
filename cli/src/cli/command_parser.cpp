#include "capcom/cli/command_parser.hpp"

namespace capcom::cli {

Command CommandParser::parse(
    const int argc,
    const char* const argv[]) const {

    if (argc < 2) {
        return {CommandType::help, {}};
    }

    const std::string_view name{argv[1]};
    Command result{};
    int first_argument = 2;

    if (name == "help" || name == "--help" || name == "-h") {
        result.type = CommandType::help;
    } else if (name == "version" || name == "--version" || name == "-v") {
        result.type = CommandType::version;
    } else if (name == "init" || name == "-i") {
        result.type = CommandType::init;
    } else if (name == "create" || name == "-c") {
        result.type = CommandType::create;
    } else if (name == "link" || name == "-l") {
        result.type = CommandType::link;
    } else if (name == "unlink" || name == "-u") {
        result.type = CommandType::unlink;
    } else if (name == "tree" || name == "-t") {
        result.type = CommandType::tree;
    } else if (name == "scan" || name == "-s") {
        result.type = CommandType::scan;
    } else if (name == "status" || name == "-st") {
        result.type = CommandType::status;
    } else if (name == "validate") {
        result.type = CommandType::validate;
    } else if (
        name == "test" &&
        argc >= 3 &&
        std::string_view{argv[2]} == "import") {
        result.type = CommandType::test_import;
        first_argument = 3;
    }

    for (int index = first_argument; index < argc; ++index) {
        result.arguments.emplace_back(argv[index]);
    }

    return result;
}

std::string_view CommandParser::help_text() noexcept {
    return
        "CapCom CLI 0.4.0\n"
        "Usage:\n"
        "  cap init [directory]                              (-i)\n"
        "  cap create <type> [title] [-p <parent_uid>]       (-c)\n"
        "  cap link <child_uid> <parent_uid>                 (-l)\n"
        "  cap unlink <child_uid> <parent_uid>               (-u)\n"
        "  cap tree                                          (-t)\n"
        "  cap scan                                          (-s)\n"
        "  cap status                                        (-st)\n"
        "  cap validate\n"
        "  cap test import <result-file>\n";
}

} // namespace capcom::cli
