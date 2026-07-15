#pragma once
#include "captain/service/requirement_creation_service.hpp"
#include "captain/storage/storage.hpp"
#include <memory>
namespace captain::repository {class HybridRepository{public:HybridRepository(std::shared_ptr<storage::IRequirementStorage>,std::shared_ptr<storage::IRequirementStorage>,std::shared_ptr<storage::ICollaborationStorage>);std::vector<domain::Requirement> all()const;domain::Requirement create(const domain::CreateRequirement&);std::vector<domain::Comment> comments(const std::string&)const;domain::Comment add_comment(const std::string&,const std::string&,const std::string&);std::vector<domain::FileLink> files(const std::string&)const;domain::FileLink add_file(const domain::FileLink&);private:std::shared_ptr<storage::IRequirementStorage> local_,remote_;std::shared_ptr<storage::ICollaborationStorage> collaboration_;service::RequirementCreationService creation_;};}
