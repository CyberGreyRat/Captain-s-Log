#pragma once
#include "capcom/identity/identity_manager.hpp"
#include <filesystem>
#include <string>
namespace capcom::audit {
class HistoryService final {
public:
    explicit HistoryService(capcom::identity::Identity identity);
    void append(const std::filesystem::path& yaml_file,
                const std::string& uid,
                const std::string& action) const;
    void verify_file(const std::filesystem::path& yaml_file,
                     const std::string& uid) const;
    void verify_project(const std::filesystem::path& project) const;
private:
    capcom::identity::Identity identity_;
};
}
