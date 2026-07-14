#include "capcom/commands/migrate_command.hpp"
#include "capcom/audit/history_service.hpp"
#include "capcom/identity/identity_manager.hpp"
#include "capcom/yaml/yaml_store.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
namespace capcom::commands {int MigrateCommand::execute(const std::filesystem::path&p)const{auto backup=p/"reqs-schema-v1-backup";if(!std::filesystem::exists(backup))std::filesystem::copy(p/"reqs",backup,std::filesystem::copy_options::recursive);auto id=capcom::identity::IdentityManager{}.load_or_create();capcom::audit::HistoryService h{id};h.migrate_project(p);auto items=capcom::yaml::YamlStore{p}.load_all();for(auto&[uid,item]:items){auto dir=p/"reqs";auto type=item.type;std::transform(type.begin(),type.end(),type.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});auto target=dir/type/item.file.filename();std::filesystem::create_directories(target.parent_path());if(item.file!=target){std::error_code e;std::filesystem::rename(item.file,target,e);if(e)throw std::runtime_error("Cannot move "+item.file.string()+": "+e.message());}}std::cout<<"Migrated project to schema version 2.\n";return 0;}}
