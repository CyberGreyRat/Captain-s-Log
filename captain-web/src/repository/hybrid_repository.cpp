#include "captain/repository/hybrid_repository.hpp"
#include <map>
#include <stdexcept>
namespace captain::repository {
HybridRepository::HybridRepository(
    std::shared_ptr<storage::IRequirementStorage> local,
    std::shared_ptr<storage::IRequirementStorage> remote,
    std::shared_ptr<storage::ICollaborationStorage> collaboration)
    : local_(std::move(local)), remote_(std::move(remote)),
      collaboration_(std::move(collaboration)), creation_(local_, remote_) {}
storage::IRequirementStorage& HybridRepository::route(const std::string& type) const {
    if (local_->writable(type)) return *local_;
    if (remote_->writable(type)) return *remote_;
    throw std::runtime_error("No storage route for type: " + type);
}
std::vector<domain::Requirement> HybridRepository::all() const {
    std::map<std::string, domain::Requirement> merged;
    for (auto& item : local_->all()) {
        if (!merged.emplace(item.uid, std::move(item)).second) {
            throw std::runtime_error("Duplicate local UID: " + item.uid);
        }
    }
    for (auto& item : remote_->all()) {
        if (merged.contains(item.uid)) {
            throw std::runtime_error("UID conflict: " + item.uid);
        }
        merged.emplace(item.uid, std::move(item));
    }
    std::vector<domain::Requirement> result;
    for (auto& [uid, item] : merged) {
        (void)uid;
        result.push_back(std::move(item));
    }
    return result;
}
domain::Requirement HybridRepository::create(const domain::CreateRequirement& request) {
    return creation_.create(request);
}
domain::Requirement HybridRepository::update(const domain::Requirement& requirement) {
    auto& storage = route(requirement.type);
    storage.update(requirement);
    const auto saved = storage.find(requirement.uid);
    if (!saved) throw std::runtime_error("Updated requirement not found.");
    return *saved;
}
std::vector<domain::Comment> HybridRepository::comments(const std::string& uid) const {
    return collaboration_->comments(uid);
}
domain::Comment HybridRepository::add_comment(
    const std::string& uid, const std::string& author, const std::string& text) {
    return collaboration_->add_comment(uid, author, text);
}
std::vector<domain::FileLink> HybridRepository::files(const std::string& uid) const {
    return collaboration_->files(uid);
}
std::vector<domain::FileLink> HybridRepository::all_files() const {
    return collaboration_->all_files();
}
domain::FileLink HybridRepository::add_file(const domain::FileLink& file) {
    return collaboration_->add_file(file);
}
domain::FileLink HybridRepository::update_file(const domain::FileLink& file) {
    return collaboration_->update_file(file);
}
}
