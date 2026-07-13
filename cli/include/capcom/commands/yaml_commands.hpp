#pragma once
#include <filesystem>
namespace capcom::commands {
class ScanCommand final{public:int execute(const std::filesystem::path&)const;};
class StatusCommand final{public:int execute(const std::filesystem::path&)const;};
class TestImportCommand final{public:int execute(const std::filesystem::path&,const std::filesystem::path&)const;};
class ValidateCommand final{public:int execute(const std::filesystem::path&)const;};
}
