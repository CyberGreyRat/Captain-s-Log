#include "captain/service/requirement_creation_service.hpp"
#include <algorithm>
#include <stdexcept>
namespace captain::service {
namespace {
void append_unique(std::vector<std::string>& values, const std::string& value) {
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}
}
RequirementCreationService::RequirementCreationService(
    std::shared_ptr<storage::IRequirementStorage> local,
    std::shared_ptr<storage::IRequirementStorage> remote)
    : local_(std::move(local)), remote_(std::move(remote)) {}
storage::IRequirementStorage& RequirementCreationService::storage_for(
    const std::string& type) const {
    if (local_->writable(type)) return *local_;
    if (remote_->writable(type)) return *remote_;
    throw std::runtime_error("No writable storage route for type: " + type);
}
std::optional<domain::Requirement> RequirementCreationService::find(
    const std::string& uid) const {
    if (const auto local = local_->find(uid)) return local;
    return remote_->find(uid);
}
void RequirementCreationService::validate_links(
    const domain::CreateRequirement& request) const {
    for (const auto& uid : request.parents) {
        if (!find(uid)) throw std::runtime_error("Unknown parent UID: " + uid);
    }
    for (const auto& uid : request.children) {
        if (!find(uid)) throw std::runtime_error("Unknown child UID: " + uid);
    }
}
void RequirementCreationService::add_child(
    const std::string& parent_uid,
    const std::string& child_uid) {
    auto item = find(parent_uid);
    if (!item) throw std::runtime_error("Parent disappeared: " + parent_uid);
    append_unique(item->children, child_uid);
    storage_for(item->type).update(*item);
}
void RequirementCreationService::add_parent(
    const std::string& child_uid,
    const std::string& parent_uid) {
    auto item = find(child_uid);
    if (!item) throw std::runtime_error("Child disappeared: " + child_uid);
    append_unique(item->parents, parent_uid);
    storage_for(item->type).update(*item);
}
void RequirementCreationService::link_bidirectionally(
    const domain::Requirement& created) {
    for (const auto& parent : created.parents) add_child(parent, created.uid);
    for (const auto& child : created.children) add_parent(child, created.uid);
}
domain::Requirement RequirementCreationService::create(
    const domain::CreateRequirement& request) {
    validate_links(request);
    auto created = storage_for(request.type).create(request);
    link_bidirectionally(created);
    return created;
}
}
