#pragma once

#include <filesystem>

namespace capcom::commands {

class PublishCommand final {
public:
    int execute(const std::filesystem::path& project) const;
};

} // namespace capcom::commands


