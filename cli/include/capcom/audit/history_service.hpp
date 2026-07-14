#pragma once

#include "capcom/identity/identity_manager.hpp"

#include <filesystem>
#include <string>

namespace capcom::audit {

class HistoryService final {
public:
    explicit HistoryService(capcom::identity::Identity identity);

    void append(
        const std::filesystem::path& yaml_file,
        const std::string& uid,
        const std::string& action,
        const std::string& reason = {}) const;

    // Verifies signatures and chain. Content changes since the latest valid
    // signature are allowed so that cap sign can authorize manual YAML edits.
    void verify_history_chain(
        const std::filesystem::path& yaml_file,
        const std::string& uid) const;

    // Verifies signatures, chain and the current YAML content hash.
    void verify_file(
        const std::filesystem::path& yaml_file,
        const std::string& uid) const;

    void verify_project(const std::filesystem::path& project) const;

    [[nodiscard]] bool has_unsigned_content_changes(
        const std::filesystem::path& yaml_file,
        const std::string& uid) const;

    [[nodiscard]] std::string current_content_hash(
        const std::filesystem::path& yaml_file) const;

    [[nodiscard]] std::string latest_signed_content_hash(
        const std::filesystem::path& yaml_file) const;

private:
    capcom::identity::Identity identity_;
};

} // namespace capcom::audit
