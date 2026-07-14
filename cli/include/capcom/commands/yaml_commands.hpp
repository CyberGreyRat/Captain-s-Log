#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace capcom::commands {class ScanCommand{public:int execute(const std::filesystem::path&)const;};class StatusCommand{public:int execute(const std::filesystem::path&)const;};class TestImportCommand{public:int execute(const std::filesystem::path&,const std::filesystem::path&)const;};class TestRunCommand{public:int execute(const std::filesystem::path&,const std::string&,const std::vector<std::string>&)const;};class ValidateCommand{public:int execute(const std::filesystem::path&)const;};}
