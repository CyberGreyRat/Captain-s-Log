#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace capcom::commands {

class CreateCommand final {
public:
    int execute(
        const std::filesystem::path& project,
        std::string type,
        const std::string& title,
        const std::optional<std::string>& parent_uid = std::nullopt) const;
};

} // namespace capcom::commands


