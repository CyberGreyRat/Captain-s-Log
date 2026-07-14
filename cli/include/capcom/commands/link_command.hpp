#pragma once
#include <filesystem>
#include <string>
namespace capcom::commands { class LinkCommand final { public: int execute(const std::filesystem::path&,const std::string&,const std::string&,bool) const; }; }


