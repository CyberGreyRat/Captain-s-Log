#pragma once
#include <filesystem>
#include <string>
namespace capcom::commands {
class SignCommand final {
public:
    int execute(const std::filesystem::path& project,
                const std::string& uid,
                const std::string& reason) const;
};
class DiffCommand final {
public:
    int execute(const std::filesystem::path& project,
                const std::string& uid) const;
};
}
