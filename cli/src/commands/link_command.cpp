#include "capcom/commands/link_command.hpp"
#include "capcom/config/app_config.hpp"
#include "capcom/requirements/requirement_store.hpp"
#include <iostream>
namespace capcom::commands {int LinkCommand::execute(const std::filesystem::path&p,const std::string&c,const std::string&parent,bool remove)const{const auto cfg=capcom::config::ConfigLoader{}.load(p);(void)cfg;capcom::requirements::RequirementStore s{p};if(remove){s.remove_link(c,parent);std::cout<<"Removed link: "<<parent<<" -> "<<c<<'\n';}else{s.add_link(c,parent);std::cout<<"Created link: "<<parent<<" -> "<<c<<'\n';}return 0;}}
