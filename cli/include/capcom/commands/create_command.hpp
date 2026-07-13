#pragma once
#include <filesystem>
#include <string>
namespace capcom::commands {
class CreateCommand final {
public:
    int execute(const std::filesystem::path& project, std::string type, const std::string& title) const;
};
}
