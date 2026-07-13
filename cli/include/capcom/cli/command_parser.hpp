#pragma once
#include <string>
#include <string_view>
#include <vector>
namespace capcom::cli {enum class CommandType{help,version,init,create,link,unlink,tree,unknown};struct Command{CommandType type{CommandType::unknown};std::vector<std::string> arguments;};class CommandParser final{public:[[nodiscard]] Command parse(int,const char*const[])const;[[nodiscard]]static std::string_view help_text()noexcept;};}
