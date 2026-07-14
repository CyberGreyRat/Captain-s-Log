#pragma once
#include <filesystem>
namespace capcom::commands { class TreeCommand final { public: int execute(const std::filesystem::path&) const; }; }


