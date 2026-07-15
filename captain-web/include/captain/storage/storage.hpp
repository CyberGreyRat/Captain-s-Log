#pragma once
#include "captain/domain/models.hpp"
#include <optional>
namespace captain::storage {
class IRequirementStorage {public:virtual ~IRequirementStorage()=default;virtual std::vector<domain::Requirement> all()const=0;virtual std::optional<domain::Requirement> find(const std::string&)const=0;virtual domain::Requirement create(const domain::CreateRequirement&)=0;virtual void update(const domain::Requirement&)=0;virtual bool writable(const std::string&)const noexcept=0;};
class ICollaborationStorage {public:virtual ~ICollaborationStorage()=default;virtual std::vector<domain::Comment> comments(const std::string&)const=0;virtual domain::Comment add_comment(const std::string&,const std::string&,const std::string&)=0;virtual std::vector<domain::FileLink> files(const std::string&)const=0;virtual domain::FileLink add_file(const domain::FileLink&)=0;};
}
