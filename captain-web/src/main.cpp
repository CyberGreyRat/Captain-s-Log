#include "captain/api/api_server.hpp"
#include "captain/repository/hybrid_repository.hpp"
#include "captain/storage/local_yaml_storage.hpp"
#include "captain/storage/sqlite_management_storage.hpp"
#include <filesystem>
#include <iostream>
#include <memory>
int main(int argc,char**argv){try{auto project=argc>1?std::filesystem::path{argv[1]}:std::filesystem::current_path();auto web=argc>2?std::filesystem::path{argv[2]}:std::filesystem::current_path()/"web";std::filesystem::create_directories(project/"reqs");auto local=std::make_shared<captain::storage::LocalYamlStorage>(project);auto remote=std::make_shared<captain::storage::SqliteManagementStorage>(project/"captain-management.db");captain::repository::HybridRepository repository{local,remote,remote};captain::sync::SyncService sync{*local,*remote};captain::api::ApiServer server{repository,sync,web};std::cout<<"Captain Web: http://127.0.0.1:8080\nProject: "<<project.string()<<'\n';server.listen("127.0.0.1",8080);return 0;}catch(const std::exception&e){std::cerr<<"Error: "<<e.what()<<'\n';return 1;}}
