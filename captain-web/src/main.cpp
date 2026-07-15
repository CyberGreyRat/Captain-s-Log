#include "captain/api/api_server.hpp"
#include "captain/auth/auth_service.hpp"
#include "captain/project/project_catalog.hpp"
#include "captain/repository/hybrid_repository.hpp"
#include "captain/storage/local_yaml_storage.hpp"
#include "captain/storage/sqlite_management_storage.hpp"
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
namespace {
std::string environment(const char*name){const char*value=std::getenv(name);return value?value:"";}
std::string project_id(std::string value){for(char&c:value){const auto u=static_cast<unsigned char>(c);if(!std::isalnum(u)&&c!='-'&&c!='_')c='-';}return value;}
}
int main(int argc,char**argv){try{
    const auto project=argc>1?std::filesystem::path{argv[1]}:std::filesystem::current_path();
    const auto web=argc>2?std::filesystem::path{argv[2]}:std::filesystem::current_path()/"web";
    const auto cap=argc>3?std::filesystem::path{argv[3]}:std::filesystem::path{"cap.exe"};
    const std::string ticket=argc>4?argv[4]:"";
    const int port=argc>5?std::stoi(argv[5]):8080;
    std::filesystem::create_directories(project/"reqs");
    const auto name=project.filename().string();const auto id=project_id(name);
    const auto user=environment("USERNAME").empty()?"developer":environment("USERNAME");
    const auto host=environment("COMPUTERNAME").empty()?"localhost":environment("COMPUTERNAME");
    auto home=environment("USERPROFILE");if(home.empty())home=project.string();
    auto local=std::make_shared<captain::storage::LocalYamlStorage>(project);
    auto remote=std::make_shared<captain::storage::SqliteManagementStorage>(project/"captain-management.db");
    captain::repository::HybridRepository repository{local,remote,remote};captain::sync::SyncService sync{*local,*remote,*remote};
    captain::project::ProjectCatalog catalog{std::filesystem::path{home}/".captain"/"captain-central.db"};
    catalog.register_project(id,name,project.string(),user);
    captain::auth::AuthService auth{ticket,user,host,id};
    captain::api::ApiServer server{repository,sync,catalog,auth,id,name,project,web,cap};
    std::cout<<"Captain Web: http://127.0.0.1:"<<port<<"\nProject: "<<name<<"\nMode: "<<(ticket.empty()?"Viewer":"Developer ticket")<<'\n';
    server.listen("127.0.0.1",port);return 0;
}catch(const std::exception&e){std::cerr<<"Error: "<<e.what()<<'\n';return 1;}}
