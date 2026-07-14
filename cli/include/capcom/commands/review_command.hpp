#pragma once

#include <filesystem>
#include <string>

namespace capcom::commands {

enum class ReviewAction {
    submit,
    approve,
    reject,
    check
};

class ReviewCommand final {
public:
    int execute(
        const std::filesystem::path& project,
        const std::string& uid,
        ReviewAction action,
        const std::string& reason = {}) const;
};

} // namespace capcom::commands


