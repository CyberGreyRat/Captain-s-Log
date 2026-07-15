#pragma once
#include "captain/storage/storage.hpp"
#include <memory>
namespace captain::service {class RequirementCreationService final{public:RequirementCreationService(std::shared_ptr<storage::IRequirementStorage>,std::shared_ptr<storage::IRequirementStorage>);domain::Requirement create(const domain::CreateRequirement&);private:std::optional<domain::Requirement>find(const std::string&)const;storage::IRequirementStorage&route(const std::string&)const;void validate(const domain::CreateRequirement&)const;bool reaches(const std::string&,const std::string&,std::vector<std::string>)const;void link(const domain::Requirement&);std::shared_ptr<storage::IRequirementStorage>local_,remote_;};}
