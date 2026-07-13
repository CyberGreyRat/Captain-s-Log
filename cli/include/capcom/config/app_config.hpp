#pragma once

#include <filesystem>
#include <string>

namespace capcom::config {

struct AppConfig {
    std::string api_url{"http://127.0.0.1:8000"};
    std::string api_key;
};

class ConfigLoader final {
public:
    [[nodiscard]] AppConfig load(
        const std::filesystem::path& project_directory) const;
};

} // namespace capcom::config
