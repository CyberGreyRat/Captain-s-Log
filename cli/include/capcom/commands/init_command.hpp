#pragma once
#include <filesystem>
namespace capcom::commands {
class InitCommand final { public: int execute(const std::filesystem::path& target) const; };
}


