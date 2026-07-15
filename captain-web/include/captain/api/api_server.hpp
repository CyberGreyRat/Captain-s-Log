#pragma once
#include "captain/api/cli_bridge.hpp"
#include "captain/repository/hybrid_repository.hpp"
#include "captain/sync/sync_service.hpp"
#include <filesystem>
#include <httplib.h>
namespace captain::api {
class ApiServer final {
public:
    ApiServer(repository::HybridRepository&,sync::SyncService&,
              std::filesystem::path project,std::filesystem::path web,
              std::filesystem::path cap_executable);
    void routes();
    void listen(const std::string&,int);
private:
    repository::HybridRepository& repository_;
    sync::SyncService& sync_;
    std::filesystem::path project_;
    std::filesystem::path web_;
    CliBridge cli_;
    httplib::Server server_;
};
}
