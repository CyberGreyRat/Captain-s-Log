#include "capcom/commands/security_commands.hpp"
#include "capcom/audit/history_service.hpp"
#include "capcom/identity/identity_manager.hpp"
#include <iostream>
namespace capcom::commands {
int VerifyCommand::execute(const std::filesystem::path& project) const {
    const auto identity = capcom::identity::IdentityManager{}.load_or_create();
    capcom::audit::HistoryService{identity}.verify_project(project);
    std::cout << "All Captain's Log audit signatures are valid.\n";
    return 0;
}
}


