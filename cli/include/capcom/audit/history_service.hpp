#pragma once
#include "capcom/identity/identity_manager.hpp"
#include <filesystem>
#include <string>
#include <vector>
namespace capcom::audit {
struct AuditEntry {
    std::string uid, timestamp, action, actor, host_id, key_id, algorithm;
    std::string public_key, content_hash, previous_signature, reason, signature, yaml_content;
};
class HistoryService final {
public:
    explicit HistoryService(capcom::identity::Identity identity);
    void append(const std::filesystem::path& yaml_file,const std::string& uid,
                const std::string& action,const std::string& reason = {}) const;
    void verify_history_chain(const std::filesystem::path& yaml_file,const std::string& uid) const;
    void verify_file(const std::filesystem::path& yaml_file,const std::string& uid) const;
    void verify_project(const std::filesystem::path& project) const;
    bool has_unsigned_content_changes(const std::filesystem::path& yaml_file,const std::string& uid) const;
    std::string current_content_hash(const std::filesystem::path& yaml_file) const;
    std::string latest_signed_content_hash(const std::filesystem::path& yaml_file) const;
    std::vector<AuditEntry> entries(const std::filesystem::path& yaml_file,const std::string& uid) const;
    std::string first_key_for_action(const std::filesystem::path& yaml_file,const std::string& uid,const std::string& action) const;
    std::string latest_key_for_action(const std::filesystem::path& yaml_file,const std::string& uid,const std::string& action) const;
    void migrate_project(const std::filesystem::path& project) const;
private: capcom::identity::Identity identity_;
};
}
