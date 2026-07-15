#pragma once
#include "captain/api/cli_bridge.hpp"
#include "captain/repository/hybrid_repository.hpp"
#include "captain/sync/sync_service.hpp"
#include <filesystem>
#include <httplib.h>
namespace captain::api {
class ApiServer final {
public:
    ApiServer(
        repository::HybridRepository& repository,
        sync::SyncService& sync,
        std::filesystem::path project,
        std::filesystem::path web,
        std::filesystem::path cap_executable);
    void routes();
    void listen(const std::string& host, int port);
private:
    std::string user() const;
    std::string host() const;
    void audit(
        const std::string& action,
        const std::string& object_uid,
        const std::string& detail) const;
    repository::HybridRepository& repository_;
    sync::SyncService& sync_;
    std::filesystem::path project_;
    std::filesystem::path web_;
    CliBridge cli_;
    httplib::Server server_;
};
}
