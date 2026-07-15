$ErrorActionPreference = "Stop"

$root = "C:\Users\luec-a\Documents\CapCom\captain-web"
$header = Join-Path $root "include\captain\api\api_server.hpp"

$content = @'
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
    [[nodiscard]] std::string user() const;
    [[nodiscard]] std::string host() const;

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
'@

[System.IO.File]::WriteAllText(
    $header,
    $content,
    [System.Text.UTF8Encoding]::new($false)
)

Write-Host "ApiServer header restored to the currently implemented v0.6 API."
Write-Host "The unfinished v0.7 auth classes remain compiled but are not active yet."
