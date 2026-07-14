#pragma once
#include <filesystem>
namespace capcom::commands {
class VerifyCommand final {
public:
    int execute(const std::filesystem::path& project) const;
};
}
