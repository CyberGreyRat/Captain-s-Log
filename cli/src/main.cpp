#include "capcom/cli/command_parser.hpp"
#include "capcom/commands/create_command.hpp"
#include "capcom/commands/init_command.hpp"
#include "capcom/commands/link_command.hpp"
#include "capcom/commands/manual_commands.hpp"
#include "capcom/commands/security_commands.hpp"
#include "capcom/commands/tree_command.hpp"
#include "capcom/commands/yaml_commands.hpp"

#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

struct CreateArguments {
    std::string type;
    std::string title;
    std::optional<std::string> parent_uid;
};

CreateArguments parse_create_arguments(
    const std::vector<std::string>& arguments) {
    if (arguments.empty()) {
        throw std::runtime_error(
            "Usage: cap create <type> [title] [-p <parent_uid>]");
    }

    CreateArguments result{};
    result.type = arguments.front();

    for (std::size_t index = 1; index < arguments.size(); ++index) {
        const auto& argument = arguments[index];

        if (argument == "-p" || argument == "--parent") {
            if (result.parent_uid.has_value()) {
                throw std::runtime_error("Parent was supplied more than once.");
            }
            if (index + 1 >= arguments.size()) {
                throw std::runtime_error("Missing UID after -p/--parent.");
            }
            result.parent_uid = arguments[++index];
            continue;
        }

        if (!result.title.empty()) {
            throw std::runtime_error(
                "Title must be quoted when it contains spaces.");
        }
        result.title = argument;
    }

    if (result.title.empty()) {
        std::cout << "Titel: " << std::flush;
        std::getline(std::cin, result.title);
        if (!std::cin || result.title.empty()) {
            throw std::runtime_error("A title is required.");
        }
    }

    return result;
}

struct SignArguments {
    std::string uid;
    std::string reason;
};

SignArguments parse_sign_arguments(
    const std::vector<std::string>& arguments) {
    if (arguments.size() != 3 ||
        (arguments[1] != "-m" && arguments[1] != "--message")) {
        throw std::runtime_error(
            "Usage: cap sign <uid> -m \"change reason\"");
    }

    return {arguments[0], arguments[2]};
}

#ifdef _WIN32

std::string wide_to_utf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int required_size = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);

    if (required_size <= 0) {
        throw std::runtime_error(
            "Windows command-line conversion to UTF-8 failed.");
    }

    std::string result(
        static_cast<std::size_t>(required_size),
        '\0');

    const int converted_size = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        required_size,
        nullptr,
        nullptr);

    if (converted_size != required_size) {
        throw std::runtime_error(
            "Windows command-line conversion to UTF-8 was incomplete.");
    }

    return result;
}

std::vector<std::string> windows_utf8_arguments() {
    int argument_count = 0;
    wchar_t** wide_arguments = CommandLineToArgvW(
        GetCommandLineW(),
        &argument_count);

    if (wide_arguments == nullptr) {
        throw std::runtime_error(
            "Windows command-line arguments could not be read.");
    }

    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argument_count));

    try {
        for (int index = 0; index < argument_count; ++index) {
            arguments.push_back(wide_to_utf8(wide_arguments[index]));
        }
    } catch (...) {
        LocalFree(wide_arguments);
        throw;
    }

    LocalFree(wide_arguments);
    return arguments;
}

#endif

} // namespace

int main(const int argc, const char* const argv[]) {
    try {
#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);

        const auto utf8_arguments = windows_utf8_arguments();
        std::vector<const char*> utf8_argv;
        utf8_argv.reserve(utf8_arguments.size());

        for (const auto& argument : utf8_arguments) {
            utf8_argv.push_back(argument.c_str());
        }

        const auto command = capcom::cli::CommandParser{}.parse(
            static_cast<int>(utf8_argv.size()),
            utf8_argv.data());
#else
        const auto command = capcom::cli::CommandParser{}.parse(argc, argv);
#endif

        switch (command.type) {
        case capcom::cli::CommandType::help:
            std::cout << capcom::cli::CommandParser::help_text();
            return 0;

        case capcom::cli::CommandType::version:
            std::cout << "Captain's Log CLI 0.6.0\n";
            return 0;

        case capcom::cli::CommandType::init: {
            if (command.arguments.size() > 1) {
                throw std::runtime_error("Usage: cap init [directory]");
            }
            const auto target = command.arguments.empty()
                ? std::filesystem::current_path()
                : std::filesystem::path{command.arguments.front()};
            return capcom::commands::InitCommand{}.execute(target);
        }

        case capcom::cli::CommandType::create: {
            const auto create = parse_create_arguments(command.arguments);
            return capcom::commands::CreateCommand{}.execute(
                std::filesystem::current_path(),
                create.type,
                create.title,
                create.parent_uid);
        }

        case capcom::cli::CommandType::link:
        case capcom::cli::CommandType::unlink:
            if (command.arguments.size() != 2) {
                throw std::runtime_error(
                    "Usage: cap link|unlink <child_uid> <parent_uid>");
            }
            return capcom::commands::LinkCommand{}.execute(
                std::filesystem::current_path(),
                command.arguments[0],
                command.arguments[1],
                command.type == capcom::cli::CommandType::unlink);

        case capcom::cli::CommandType::tree:
            if (!command.arguments.empty()) {
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
            if (command.arguments.size() != 1) {
                throw std::runtime_error(
                    "Usage: cap test import <result-file>");
            }
            return capcom::commands::TestImportCommand{}.execute(
                std::filesystem::current_path(),
                command.arguments.front());

        case capcom::cli::CommandType::verify:
            if (!command.arguments.empty()) {
                throw std::runtime_error("Usage: cap verify");
            }
            return capcom::commands::VerifyCommand{}.execute(
                std::filesystem::current_path());

        case capcom::cli::CommandType::diff:
            if (command.arguments.size() != 1) {
                throw std::runtime_error("Usage: cap diff <uid>");
            }
            return capcom::commands::DiffCommand{}.execute(
                std::filesystem::current_path(),
                command.arguments.front());

        case capcom::cli::CommandType::sign: {
            const auto sign = parse_sign_arguments(command.arguments);
            return capcom::commands::SignCommand{}.execute(
                std::filesystem::current_path(),
                sign.uid,
                sign.reason);
        }

        case capcom::cli::CommandType::unknown:
            throw std::runtime_error("Unknown command. Run 'cap help'.");
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 2;
    }

    return 2;
}
