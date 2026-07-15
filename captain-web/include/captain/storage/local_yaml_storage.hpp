#pragma once
#include "captain/storage/storage.hpp"
#include <filesystem>
namespace captain::storage {class LocalYamlStorage final:public IRequirementStorage{public:explicit LocalYamlStorage(std::filesystem::path);std::vector<domain::Requirement>all()const override;std::optional<domain::Requirement>find(const std::string&)const override;domain::Requirement create(const domain::CreateRequirement&)override;void update(const domain::Requirement&)override;bool writable(const std::string&)const noexcept override;void write_mirror(const domain::Requirement&);private:std::filesystem::path project_;};}
