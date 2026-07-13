#include "capcom/commands/create_command.hpp"

#include "capcom/config/app_config.hpp"
#include "capcom/requirements/requirement_store.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace capcom::commands {
namespace {

std::string yaml_quote(const std::string& value) {
    std::string result{"\""};
    for (const char character : value) {
        if (character == '\\' || character == '"') result.push_back('\\');
        if (character == '\n') result += "\\n";
        else if (character != '\r') result.push_back(character);
    }
    result.push_back('"');
    return result;
}

bool valid_type(const std::string& type) {
    return type.size() >= 2 && type.size() <= 16 &&
        std::all_of(type.begin(), type.end(), [](const unsigned char character) {
            return std::isalnum(character) != 0 || character == '_';
        });
}

} // namespace

int CreateCommand::execute(
    const std::filesystem::path& project,
    std::string type,
    const std::string& title,
    const std::optional<std::string>& parent_uid) const {

    const auto config = capcom::config::ConfigLoader{}.load(project);
    (void)config;

    std::transform(type.begin(), type.end(), type.begin(), [](const unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });

    if (!valid_type(type)) {
        throw std::runtime_error("Type must contain 2-16 letters, digits or underscores.");
    }
    if (title.empty() || title.size() > 300) {
        throw std::runtime_error("Title must contain 1-300 characters.");
    }

    const auto requirements_directory = project / "reqs";
    std::filesystem::create_directories(requirements_directory);

    const std::regex file_pattern{"^" + type + R"(-(\d+)\.yaml$)"};
    int highest_number = 0;

    for (const auto& entry : std::filesystem::directory_iterator(requirements_directory)) {
        if (!entry.is_regular_file()) continue;

        std::smatch match;
        const auto filename = entry.path().filename().string();
        if (std::regex_match(filename, match, file_pattern)) {
            highest_number = std::max(highest_number, std::stoi(match[1].str()));
        }
    }

    std::ostringstream uid_builder;
    uid_builder << type << '-' << std::setw(3) << std::setfill('0') << highest_number + 1;
    const auto uid = uid_builder.str();
    const auto file = requirements_directory / (uid + ".yaml");

    {
        std::ofstream output(file, std::ios::binary);
        if (!output) throw std::runtime_error("Cannot create " + file.string());

        output << "uid: " << uid << "\n"
               << "type: " << type << "\n"
               << "status: Draft\n"
               << "title: " << yaml_quote(title) << "\n"
               << "text: \"\"\n"
               << "rationale: \"\"\n"
               << "version: \"0.1.0\"\n"
               << "git_hash: null\n"
               << "attributes: {}\n"
               << "parents: []\n"
               << "children: []\n"
               << "integration_template: |\n"
               << "  /* @cap-start: " << uid << "\n"
               << "   * @cap-desc: " << title << "\n"
               << "   */\n"
               << "  /* @cap-end: " << uid << " */\n";

        if (!output) throw std::runtime_error("Failed while writing " + file.string());
    }

    try {
        if (parent_uid.has_value()) {
            capcom::requirements::RequirementStore{project}.add_link(uid, *parent_uid);
        }
    } catch (...) {
        // Do not leave a half-created, unlinked item when the combined command fails.
        std::error_code ignored;
        std::filesystem::remove(file, ignored);
        throw;
    }

    std::cout << "Created " << uid << ": " << file.string() << '\n';
    if (parent_uid.has_value()) {
        std::cout << "Linked " << *parent_uid << " -> " << uid << '\n';
    }
    return 0;
}

} // namespace capcom::commands
