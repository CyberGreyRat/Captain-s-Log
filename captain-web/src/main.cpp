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
namespace {std::string environment(const char*n){const char*v=std::getenv(n);return v?v:"";}std::string make_id(std::string v){for(char&c:v){auto u=static_cast<unsigned char>(c);if(!std::isalnum(u)&&c!='-'&&c!='_')c='-';}return v;}}
int main(int argc,char**argv){try{const auto project=argc>1?std::filesystem::path{argv[1]}:std::filesystem::current_path();const auto web=argc>2?std::filesystem::path{argv[2]}:std::filesystem::current_path()/"web";const auto cap=argc>3?std::filesystem::path{argv[3]}:std::filesystem::path{"cap.exe"};const std::string ticket=argc>4?argv[4]:"";const int port=argc>5?std::stoi(argv[5]):8080;std::filesystem::create_directories(project/"reqs");const auto name=project.filename().string();const auto id=make_id(name);auto username=environment("USERNAME");if(username.empty())username="developer";auto hostname=environment("COMPUTERNAME");if(hostname.empty())hostname="localhost";auto home=environment("USERPROFILE");if(home.empty())home=project.string();auto local=std::make_shared<captain::storage::LocalYamlStorage>(project);auto remote=std::make_shared<captain::storage::SqliteManagementStorage>(project/"captain-management.db");captain::repository::HybridRepository repository{local,remote,remote};captain::sync::SyncService sync{*local,*remote,*remote};captain::project::ProjectCatalog catalog{std::filesystem::path{home}/".captain"/"captain-central.db"};catalog.register_project(id,name,project.string(),username);captain::auth::AuthService auth{ticket,username,hostname,id};captain::api::ApiServer server{repository,sync,catalog,auth,id,name,project,web,cap};std::cout<<"Captain Web: http://127.0.0.1:"<<port<<"\nProject: "<<name<<"\nMode: "<<(ticket.empty()?"Viewer":"Developer ticket")<<'\n';server.listen("127.0.0.1",port);return 0;}catch(const std::exception&e){std::cerr<<"Error: "<<e.what()<<'\n';return 1;}}
