#pragma once
#include <filesystem>
#include <string>
namespace captain::api {struct CommandResult{int exit_code{};std::string output;};class CliBridge final{public:CliBridge(std::filesystem::path,std::filesystem::path);CommandResult scan()const;CommandResult verify()const;CommandResult validate()const;CommandResult status()const;CommandResult tree()const;CommandResult publish()const;CommandResult sign(const std::string&,const std::string&)const;private:CommandResult run(const std::string&)const;std::filesystem::path project_,executable_;};}
