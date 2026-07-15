#pragma once
#include "captain/service/requirement_creation_service.hpp"
#include "captain/storage/storage.hpp"
#include <memory>
namespace captain::repository {
class HybridRepository {
public:
    HybridRepository(
        std::shared_ptr<storage::IRequirementStorage> local,
        std::shared_ptr<storage::IRequirementStorage> remote,
        std::shared_ptr<storage::ICollaborationStorage> collaboration);
    std::vector<domain::Requirement> all() const;
    domain::Requirement create(const domain::CreateRequirement& request);
    domain::Requirement update(const domain::Requirement& requirement);
    std::vector<domain::Comment> comments(const std::string& uid) const;
    domain::Comment add_comment(
        const std::string& uid,
        const std::string& author,
        const std::string& text);
    std::vector<domain::FileLink> files(const std::string& uid) const;
    std::vector<domain::FileLink> all_files() const;
    domain::FileLink add_file(const domain::FileLink& file);
    domain::FileLink update_file(const domain::FileLink& file);
private:
    storage::IRequirementStorage& route(const std::string& type) const;
    std::shared_ptr<storage::IRequirementStorage> local_;
    std::shared_ptr<storage::IRequirementStorage> remote_;
    std::shared_ptr<storage::ICollaborationStorage> collaboration_;
    service::RequirementCreationService creation_;
};
}
