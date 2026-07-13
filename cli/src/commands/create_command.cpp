#include "capcom/commands/create_command.hpp"
#include "capcom/config/app_config.hpp"
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
    for (const char c : value) {
        if (c == '\\' || c == '"') result.push_back('\\');
        if (c == '\n') result += "\\n"; else if (c != '\r') result.push_back(c);
    }
    result.push_back('"'); return result;
}
bool valid_type(const std::string& type) {
    return type.size() >= 2 && type.size() <= 16 &&
        std::all_of(type.begin(), type.end(), [](unsigned char c){ return std::isalnum(c) || c == '_'; });
}
}
int CreateCommand::execute(const std::filesystem::path& project, std::string type, const std::string& title) const {
    const auto config = capcom::config::ConfigLoader{}.load(project);
    (void)config;
    std::transform(type.begin(), type.end(), type.begin(), [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    if (!valid_type(type)) throw std::runtime_error("Type must contain 2-16 letters, digits or underscores.");
    if (title.empty() || title.size() > 300) throw std::runtime_error("Title must contain 1-300 characters.");
    const auto reqs = project / "reqs"; std::filesystem::create_directories(reqs);
    const std::regex pattern{"^" + type + R"(-(\d+)\.yaml$)"};
    int highest = 0;
    for (const auto& entry : std::filesystem::directory_iterator(reqs)) {
        std::smatch match; const auto name = entry.path().filename().string();
        if (entry.is_regular_file() && std::regex_match(name, match, pattern)) highest = std::max(highest, std::stoi(match[1].str()));
    }
    std::ostringstream uid; uid << type << '-' << std::setw(3) << std::setfill('0') << highest + 1;
    const auto file = reqs / (uid.str() + ".yaml");
    std::ofstream out(file, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot create " + file.string());
    out << "uid: " << uid.str() << "\n"
        << "type: " << type << "\nstatus: Draft\ntitle: " << yaml_quote(title)
        << "\ntext: \"\"\nrationale: \"\"\nversion: \"0.1.0\"\ngit_hash: null\n"
        << "attributes: {}\nintegration_template: |\n"
        << "  /* @cap-start: " << uid.str() << "\n"
        << "   * @cap-desc: " << title << "\n   */\n"
        << "  /* @cap-end: " << uid.str() << " */\n";
    out.close();
    if (!out) throw std::runtime_error("Failed while writing " + file.string());
    std::cout << "Created " << uid.str() << ": " << file.string() << '\n';
    return 0;
}
}
