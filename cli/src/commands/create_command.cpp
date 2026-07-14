#include "capcom/commands/create_command.hpp"

#include "capcom/audit/history_service.hpp"
#include "capcom/config/app_config.hpp"
#include "capcom/identity/identity_manager.hpp"
#include "capcom/requirements/requirement_store.hpp"
#include "capcom/yaml/yaml_store.hpp"

#include <algorithm>
#include <array>
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
        if (character == '\\' || character == '"') {
            result.push_back('\\');
        }
        if (character == '\n') {
            result += "\\n";
        } else if (character != '\r') {
            result.push_back(character);
        }
    }
    result.push_back('"');
    return result;
}

std::string lowercase(std::string value) {
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

bool supported_type(const std::string& type) {
    constexpr std::array supported{
        "USR", "SYS", "SRS", "SEC", "ARCH", "LLD", "RISK", "CLASS",
        "TEST", "UT", "IT", "ST", "AT", "BUG", "CR", "SOUP"
    };
    return std::find(supported.begin(), supported.end(), type) != supported.end();
}

void write_attributes(std::ostream& output, const std::string& type) {
    output << "attributes:\n";

    if (type == "CLASS") {
        output
            << "  software_system: \"\"\n"
            << "  software_safety_class: \"\"\n"
            << "  classification_responsible: \"\"\n"
            << "  classification_rationale: \"\"\n"
            << "  risk_management_document: \"\"\n"
            << "  risk_management_record_ids: []\n";
        return;
    }

    if (type == "SRS" || type == "SEC") {
        output
            << "  classification_ref: \"\"\n"
            << "  software_item_safety_class: \"\"\n"
            << "  classification_responsible: \"\"\n"
            << "  classification_rationale: \"\"\n"
            << "  risk_references: []\n";
        return;
    }

    if (type == "RISK") {
        output
            << "  hazard: \"\"\n"
            << "  hazardous_situation: \"\"\n"
            << "  harm: \"\"\n"
            << "  risk_responsible: \"\"\n"
            << "  initial_severity: \"\"\n"
            << "  initial_probability: \"\"\n"
            << "  control_requirements: []\n"
            << "  verification_tests: []\n"
            << "  external_document: \"\"\n"
            << "  external_sheet: \"\"\n"
            << "  external_record_id: \"\"\n"
            << "  external_version: \"\"\n";
        return;
    }

    if (type == "UT" || type == "IT" || type == "ST" ||
        type == "AT" || type == "TEST") {
        output
            << "  verifies: []\n"
            << "  test_method: \"\"\n"
            << "  expected_result: \"\"\n"
            << "  test_responsible: \"\"\n";
        return;
    }

    output << "  owner: \"\"\n";
}

} // namespace

int CreateCommand::execute(
    const std::filesystem::path& project,
    std::string type,
    const std::string& title,
    const std::optional<std::string>& parent_uid) const {
    const auto configuration = capcom::config::ConfigLoader{}.load(project);
    (void)configuration;

    std::transform(
        type.begin(), type.end(), type.begin(),
        [](const unsigned char character) {
            return static_cast<char>(std::toupper(character));
        });

    if (!supported_type(type)) {
        throw std::runtime_error("Unsupported type: " + type);
    }
    if (title.empty() || title.size() > 300) {
        throw std::runtime_error("Title must contain 1-300 characters.");
    }

    const auto directory = project / "reqs" / lowercase(type);
    std::filesystem::create_directories(directory);

    const std::regex pattern{"^" + type + R"(-(\d+)\.yaml$)"};
    int highest_number = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        std::smatch match;
        const auto filename = entry.path().filename().string();
        if (entry.is_regular_file() && std::regex_match(filename, match, pattern)) {
            highest_number = std::max(highest_number, std::stoi(match[1].str()));
        }
    }

    std::ostringstream uid_builder;
    uid_builder << type << '-' << std::setw(3) << std::setfill('0')
                << highest_number + 1;
    const auto uid = uid_builder.str();
    const auto file = directory / (uid + ".yaml");

    {
        std::ofstream output(file, std::ios::binary);
        if (!output) {
            throw std::runtime_error("Cannot create " + file.string());
        }
        output
            << "uid: " << uid << "\n"
            << "type: " << type << "\n"
            << "status: Draft\n"
            << "title: " << yaml_quote(title) << "\n"
            << "text: \"\"\n"
            << "rationale: \"\"\n"
            << "version: \"0.1.0\"\n"
            << "git_hash: null\n";
        write_attributes(output, type);
        output
            << "parents: []\n"
            << "children: []\n"
            << "integration_template: |\n"
            << "  /* @cap-start: " << uid << "\n"
            << "   * @cap-desc: " << title << "\n"
            << "   */\n"
            << "  /* @cap-end: " << uid << " */\n";
    }

    const auto identity = capcom::identity::IdentityManager{}.load_or_create();
    capcom::audit::HistoryService history{identity};
    history.append(file, uid, "ITEM_CREATED");

    try {
        if (parent_uid.has_value()) {
            capcom::requirements::RequirementStore{project}.add_link(uid, *parent_uid);
            const auto items = capcom::yaml::YamlStore{project}.load_all();
            const auto parent = items.at(capcom::yaml::normalize_uid(*parent_uid));
            const auto reason = parent.uid + " linked to " + uid;
            history.append(file, uid, "LINK_CREATED", reason);
            history.append(parent.file, parent.uid, "LINK_CREATED", reason);
        }
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(file, ignored);
        throw;
    }

    std::cout << "Created " << uid << ": " << file.string() << '\n';
    return 0;
}

} // namespace capcom::commands
