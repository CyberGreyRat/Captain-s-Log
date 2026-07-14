#include "capcom/commands/link_command.hpp"
#include "capcom/audit/history_service.hpp"
#include "capcom/config/app_config.hpp"
#include "capcom/identity/identity_manager.hpp"
#include "capcom/requirements/requirement_store.hpp"
#include "capcom/yaml/yaml_store.hpp"
#include <iostream>
#include <stdexcept>
namespace capcom::commands {
int LinkCommand::execute(const std::filesystem::path& project,
                         const std::string& child_raw,
                         const std::string& parent_raw,
                         const bool remove) const {
    const auto config = capcom::config::ConfigLoader{}.load(project);
    (void)config;
    const auto child_uid = capcom::yaml::normalize_uid(child_raw);
    const auto parent_uid = capcom::yaml::normalize_uid(parent_raw);
    const auto before = capcom::yaml::YamlStore{project}.load_all();
    const auto child_before = before.find(child_uid);
    const auto parent_before = before.find(parent_uid);
    if (child_before == before.end()) throw std::runtime_error("Child not found: " + child_uid);
    if (parent_before == before.end()) throw std::runtime_error("Parent not found: " + parent_uid);

    capcom::requirements::RequirementStore store{project};
    if (remove) store.remove_link(child_uid, parent_uid);
    else store.add_link(child_uid, parent_uid);

    const auto identity = capcom::identity::IdentityManager{}.load_or_create();
    capcom::audit::HistoryService history{identity};
    const auto action = remove ? "LINK_REMOVED" : "LINK_CREATED";
    const auto reason = parent_uid + (remove ? " unlinked from " : " linked to ") + child_uid;
    history.append(child_before->second.file, child_uid, action, reason);
    history.append(parent_before->second.file, parent_uid, action, reason);
    history.verify_file(child_before->second.file, child_uid);
    history.verify_file(parent_before->second.file, parent_uid);

    std::cout << (remove ? "Removed link: " : "Created link: ")
              << parent_uid << " -> " << child_uid << '\n';
    return 0;
}
}
