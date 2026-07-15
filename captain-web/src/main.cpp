#include "captain/api/api_server.hpp"
#include "captain/repository/hybrid_repository.hpp"
#include "captain/storage/local_yaml_storage.hpp"
#include "captain/storage/sqlite_management_storage.hpp"
#include <filesystem>
#include <iostream>
#include <memory>
int main(int argc,char**argv){try{auto project=argc>1?std::filesystem::path{argv[1]}:std::filesystem::current_path();auto web=argc>2?std::filesystem::path{argv[2]}:std::filesystem::current_path()/"web";auto cap=argc>3?std::filesystem::path{argv[3]}:std::filesystem::path{"cap.exe"};auto local=std::make_shared<captain::storage::LocalYamlStorage>(project);auto remote=std::make_shared<captain::storage::SqliteManagementStorage>(project/"captain-management.db");captain::repository::HybridRepository repo{local,remote,remote};captain::sync::SyncService sync{*local,*remote,*remote};captain::api::ApiServer server{repo,sync,project,web,cap};std::cout<<"Captain Web: http://127.0.0.1:8080\nProject: "<<project.string()<<'\n';server.listen("127.0.0.1",8080);return 0;}catch(const std::exception&e){std::cerr<<"Error: "<<e.what()<<'\n';return 1;}}
