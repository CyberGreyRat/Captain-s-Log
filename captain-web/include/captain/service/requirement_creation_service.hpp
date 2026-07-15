#pragma once
#include "captain/domain/models.hpp"
#include "captain/storage/storage.hpp"
#include <memory>
namespace captain::service {
class RequirementCreationService final {
public:
    RequirementCreationService(
        std::shared_ptr<storage::IRequirementStorage> local,
        std::shared_ptr<storage::IRequirementStorage> remote);
    domain::Requirement create(const domain::CreateRequirement& request);
private:
    void validate_links(const domain::CreateRequirement& request) const;
    void link_bidirectionally(const domain::Requirement& created);
    void add_child(const std::string& parent_uid, const std::string& child_uid);
    void add_parent(const std::string& child_uid, const std::string& parent_uid);
    storage::IRequirementStorage& storage_for(const std::string& type) const;
    std::optional<domain::Requirement> find(const std::string& uid) const;
    std::shared_ptr<storage::IRequirementStorage> local_;
    std::shared_ptr<storage::IRequirementStorage> remote_;
};
}
