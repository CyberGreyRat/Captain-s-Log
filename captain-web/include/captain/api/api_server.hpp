#pragma once
#include "captain/repository/hybrid_repository.hpp"
#include "captain/sync/sync_service.hpp"
#include <filesystem>
#include <httplib.h>
namespace captain::api {class ApiServer{public:ApiServer(repository::HybridRepository&,sync::SyncService&,std::filesystem::path);void routes();void listen(const std::string&,int);private:repository::HybridRepository& repository_;sync::SyncService& sync_;std::filesystem::path web_;httplib::Server server_;};}
