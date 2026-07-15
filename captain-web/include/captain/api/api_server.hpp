#pragma once
#include "captain/api/cli_bridge.hpp"
#include "captain/auth/auth_service.hpp"
#include "captain/project/project_catalog.hpp"
#include "captain/repository/hybrid_repository.hpp"
#include "captain/sync/sync_service.hpp"
#include <filesystem>
#include <httplib.h>
namespace captain::api {
class ApiServer final {
public:
    ApiServer(repository::HybridRepository&,sync::SyncService&,project::ProjectCatalog&,auth::AuthService&,std::string project_id,std::string project_name,std::filesystem::path project,std::filesystem::path web,std::filesystem::path cap_executable);
    void routes();
    void listen(const std::string&,int);
private:
    [[nodiscard]] std::string user() const;
    [[nodiscard]] std::string host() const;
    [[nodiscard]] std::optional<auth::Session> developer(const httplib::Request&) const;
    bool require_developer(const httplib::Request&,httplib::Response&) const;
    void audit(const std::string&,const std::string&,const std::string&) const;
    repository::HybridRepository& repository_;sync::SyncService& sync_;project::ProjectCatalog& catalog_;auth::AuthService& auth_;
    std::string project_id_,project_name_;std::filesystem::path project_,web_;CliBridge cli_;httplib::Server server_;
};
}
