#include "capcom/cli/command_parser.hpp"
#include "capcom/commands/create_command.hpp"
#include "capcom/commands/init_command.hpp"
#include "capcom/commands/link_command.hpp"
#include "capcom/commands/tree_command.hpp"
#include "capcom/commands/yaml_commands.hpp"

#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

    struct CreateArguments
    {
        std::string type;
        std::string title;
        std::optional<std::string> parent_uid;
    };

    CreateArguments parse_create_arguments(const std::vector<std::string> &arguments)
    {
        if (arguments.empty())
        {
            throw std::runtime_error(
                "Usage: cap create <type> [title] [-p <parent_uid>]");
        }

        CreateArguments result{};
        result.type = arguments.front();

        for (std::size_t index = 1; index < arguments.size(); ++index)
        {
            const auto &argument = arguments[index];

            if (argument == "-p" || argument == "--parent")
            {
                if (result.parent_uid.has_value())
                {
                    throw std::runtime_error("Parent was supplied more than once.");
                }
                if (index + 1 >= arguments.size())
                {
                    throw std::runtime_error("Missing UID after -p/--parent.");
                }
                result.parent_uid = arguments[++index];
                continue;
            }

            if (!result.title.empty())
            {
                throw std::runtime_error(
                    "Title must be quoted when it contains spaces, for example: "
                    "cap -c sys \"Temperatur messen\" -p USR-001");
            }
            result.title = argument;
        }

        if (result.title.empty())
        {
            std::cout << "Titel: " << std::flush;
            std::getline(std::cin, result.title);
            if (!std::cin || result.title.empty())
            {
                throw std::runtime_error("A title is required.");
            }
        }

        return result;
    }

} // namespace

int main(const int argc, const char *const argv[])
{
    try
    {
        const auto command = capcom::cli::CommandParser{}.parse(argc, argv);

        switch (command.type)
        {
        case capcom::cli::CommandType::help:
            std::cout << capcom::cli::CommandParser::help_text();
            return 0;

        case capcom::cli::CommandType::version:
            std::cout << "CapCom CLI 0.3.3\n";
            return 0;

        case capcom::cli::CommandType::init:
        {
            if (command.arguments.size() > 1)
            {
                throw std::runtime_error("Usage: cap init [directory]");
            }
            const auto target = command.arguments.empty()
                                    ? std::filesystem::current_path()
                                    : std::filesystem::path{command.arguments.front()};
            return capcom::commands::InitCommand{}.execute(target);
        }

        case capcom::cli::CommandType::create:
        {
            const auto create = parse_create_arguments(command.arguments);
            return capcom::commands::CreateCommand{}.execute(
                std::filesystem::current_path(),
                create.type,
                create.title,
                create.parent_uid);
        }

        case capcom::cli::CommandType::link:
        case capcom::cli::CommandType::unlink:
            if (command.arguments.size() != 2)
            {
                throw std::runtime_error(
                    "Usage: cap link|unlink <child_uid> <parent_uid>");
            }
            return capcom::commands::LinkCommand{}.execute(
                std::filesystem::current_path(),
                command.arguments[0],
                command.arguments[1],
                command.type == capcom::cli::CommandType::unlink);

        case capcom::cli::CommandType::tree:
            if (!command.arguments.empty())
            {
                throw std::runtime_error("Usage: cap tree");
            }
            return capcom::commands::TreeCommand{}.execute(
                std::filesystem::current_path());

        case capcom::cli::CommandType::scan:
            return capcom::commands::ScanCommand{}.execute(
                std::filesystem::current_path());

        case capcom::cli::CommandType::status:
            return capcom::commands::StatusCommand{}.execute(
                std::filesystem::current_path());

        case capcom::cli::CommandType::validate:
            return capcom::commands::ValidateCommand{}.execute(
                std::filesystem::current_path());

        case capcom::cli::CommandType::test_import:
            if (command.arguments.size() != 1)
            {
                throw std::runtime_error(
                    "Usage: cap test import <result-file>");
            }

            return capcom::commands::TestImportCommand{}.execute(
                std::filesystem::current_path(),
                command.arguments.front());

        case capcom::cli::CommandType::unknown:
            throw std::runtime_error("Unknown command. Run 'cap help'.");
        }
    }
    catch (const std::exception &error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        return 2;
    }

    return 2;
}
