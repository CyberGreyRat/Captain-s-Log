#pragma once
#include "captain/domain/models.hpp"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
struct sqlite3;
namespace captain::project {
class ProjectCatalog final {
public:
    explicit ProjectCatalog(const std::filesystem::path& database);
    ~ProjectCatalog();
    void register_project(const std::string&id,const std::string&name,const std::string&path,const std::string&actor);
    void push(const std::string&project_id,const std::vector<domain::Requirement>&items,const std::string&commit,const std::string&actor);
    nlohmann::json projects()const;
    nlohmann::json snapshots(const std::string&project_id)const;
private:void schema();sqlite3*db_{};
};
}
