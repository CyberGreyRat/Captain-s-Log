#pragma once
#include <filesystem>
#include <optional>
#include <string>
namespace capcom::commands {class SignCommand{public:int execute(const std::filesystem::path&,const std::string&,const std::string&)const;};class DiffCommand{public:int execute(const std::filesystem::path&,const std::optional<std::string>&)const;};}
