#pragma once
#include "captain/storage/storage.hpp"
#include <filesystem>
struct sqlite3;
namespace captain::storage {class SqliteManagementStorage final:public IRequirementStorage,public ICollaborationStorage{public:explicit SqliteManagementStorage(const std::filesystem::path&);~SqliteManagementStorage()override;std::vector<domain::Requirement> all()const override;std::optional<domain::Requirement> find(const std::string&)const override;domain::Requirement create(const domain::CreateRequirement&)override;void update(const domain::Requirement&)override;bool writable(const std::string&)const noexcept override;std::vector<domain::Comment> comments(const std::string&)const override;domain::Comment add_comment(const std::string&,const std::string&,const std::string&)override;std::vector<domain::FileLink> files(const std::string&)const override;domain::FileLink add_file(const domain::FileLink&)override;private:void schema();sqlite3* db_{};};}
