#include "capcom/cli/command_parser.hpp"
#include "capcom/commands/create_command.hpp"
#include "capcom/commands/init_command.hpp"
#include "capcom/commands/link_command.hpp"
#include "capcom/commands/tree_command.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
int main(int n,const char*const a[]){try{auto c=capcom::cli::CommandParser{}.parse(n,a);switch(c.type){case capcom::cli::CommandType::help:std::cout<<capcom::cli::CommandParser::help_text();return 0;case capcom::cli::CommandType::version:std::cout<<"CapCom CLI 0.3.2\n";return 0;case capcom::cli::CommandType::init:{if(c.arguments.size()>1)throw std::runtime_error("Usage: cap init [directory]");auto p=c.arguments.empty()?std::filesystem::current_path():std::filesystem::path{c.arguments[0]};return capcom::commands::InitCommand{}.execute(p);}case capcom::cli::CommandType::create:if(c.arguments.size()!=2)throw std::runtime_error("Usage: cap create <type> <title>");return capcom::commands::CreateCommand{}.execute(std::filesystem::current_path(),c.arguments[0],c.arguments[1]);case capcom::cli::CommandType::link:case capcom::cli::CommandType::unlink:if(c.arguments.size()!=2)throw std::runtime_error("Usage: cap link|unlink <child_uid> <parent_uid>");return capcom::commands::LinkCommand{}.execute(std::filesystem::current_path(),c.arguments[0],c.arguments[1],c.type==capcom::cli::CommandType::unlink);case capcom::cli::CommandType::tree:if(!c.arguments.empty())throw std::runtime_error("Usage: cap tree");return capcom::commands::TreeCommand{}.execute(std::filesystem::current_path());case capcom::cli::CommandType::unknown:throw std::runtime_error("Unknown command. Run 'cap help'.");}}catch(const std::exception&e){std::cerr<<"Error: "<<e.what()<<'\n';return 2;}return 2;}
