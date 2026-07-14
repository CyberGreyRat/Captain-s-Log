#include "capcom/commands/manual_commands.hpp"
#include "capcom/audit/history_service.hpp"
#include "capcom/identity/identity_manager.hpp"
#include "capcom/yaml/yaml_store.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
namespace capcom::commands {
namespace {
capcom::yaml::Item find_item(const std::filesystem::path& project,
                             const std::string& raw_uid) {
    const auto uid = capcom::yaml::normalize_uid(raw_uid);
    const auto items = capcom::yaml::YamlStore{project}.load_all();
    const auto iterator = items.find(uid);
    if (iterator == items.end()) throw std::runtime_error("Item not found: " + uid);
    return iterator->second;
}
}
int SignCommand::execute(const std::filesystem::path& project,
                         const std::string& raw_uid,
                         const std::string& reason) const {
    if (reason.empty()) throw std::runtime_error("A change reason is required.");
    const auto item = find_item(project, raw_uid);
    const auto identity = capcom::identity::IdentityManager{}.load_or_create();
    capcom::audit::HistoryService history{identity};
    history.verify_history_chain(item.file, item.uid);
    if (!history.has_unsigned_content_changes(item.file, item.uid)) {
        std::cout << item.uid << " has no unsigned content changes.\n";
        return 0;
    }
    history.append(item.file, item.uid, "CONTENT_UPDATED", reason);
    history.verify_file(item.file, item.uid);
    std::cout << "Signed manual YAML changes for " << item.uid << ".\n";
    return 0;
}
int DiffCommand::execute(const std::filesystem::path& project,
                         const std::string& raw_uid) const {
    const auto item = find_item(project, raw_uid);
    const auto identity = capcom::identity::IdentityManager{}.load_or_create();
    capcom::audit::HistoryService history{identity};
    history.verify_history_chain(item.file, item.uid);
    const auto current = history.current_content_hash(item.file);
    const auto signed_hash = history.latest_signed_content_hash(item.file);
    if (current == signed_hash) {
        std::cout << item.uid << " has no unsigned content changes.\n";
        return 0;
    }
    std::cout << "Unsigned changes detected in " << item.uid << "\n"
              << "Last signed hash: " << signed_hash << "\n"
              << "Current hash:     " << current << "\n\n";
    const auto command = "git --no-pager diff -- \"" + item.file.string() + "\"";
    const int result = std::system(command.c_str());
    if (result != 0) {
        std::cout << "Git diff is unavailable. Review the YAML manually before signing.\n";
    }
    std::cout << "To authorize the reviewed edit:\n"
              << "  cap sign " << item.uid << " -m \"Change reason\"\n";
    return 1;
}
}
