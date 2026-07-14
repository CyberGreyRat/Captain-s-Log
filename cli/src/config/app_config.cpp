#include "capcom/config/app_config.hpp"
#include <fstream>
#include <stdexcept>
namespace capcom::config {
AppConfig ConfigLoader::load(const std::filesystem::path& project_directory) const {
    const auto file = project_directory / ".capcom_config.json";
    if (!std::filesystem::is_regular_file(file)) throw std::runtime_error("Not a Captain's Log project. Run 'cap init' first.");
    return {};
}
void ConfigLoader::write_default(const std::filesystem::path& project_directory) const {
    const auto path = project_directory / ".capcom_config.json";
    if (std::filesystem::exists(path)) return;
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot create " + path.string());
    out << "{\n  \"schema_version\": 2,\n  \"api_url\": \"http://127.0.0.1:8000\",\n"
           "  \"requirements_directory\": \"reqs\"\n}\n";
}
}


