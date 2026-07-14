#include "capcom/commands/review_command.hpp"

#include "capcom/audit/history_service.hpp"
#include "capcom/identity/identity_manager.hpp"
#include "capcom/yaml/yaml_store.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace capcom::commands {
namespace {

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}


std::vector<std::string> read_lines(const std::filesystem::path& file) {
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot read " + file.string());
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

void save_atomic(
    const std::filesystem::path& file,
    const std::vector<std::string>& lines) {
    auto temporary = file;
    temporary += ".review.tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("Cannot write " + temporary.string());
        for (const auto& line : lines) output << line << '\n';
        if (!output) throw std::runtime_error("Failed writing " + temporary.string());
    }
    std::error_code error;
    std::filesystem::remove(file, error);
    error.clear();
    std::filesystem::rename(temporary, file, error);
    if (error) throw std::runtime_error("Cannot update review status: " + error.message());
}

void replace_status(
    const std::filesystem::path& file,
    const std::string& status) {
    auto lines = read_lines(file);
    const auto iterator = std::find_if(lines.begin(), lines.end(), [](const auto& line) {
        return line.rfind("status:", 0) == 0;
    });
    if (iterator == lines.end()) throw std::runtime_error("Missing status field in " + file.string());
    *iterator = "status: " + status;
    save_atomic(file, lines);
}

bool technical_type(const std::string& type) {
    return type == "SRS" || type == "SEC" || type == "ARCH" ||
           type == "LLD" || type == "UT" || type == "IT" ||
           type == "ST" || type == "AT" || type == "TEST";
}

bool test_type(const std::string& type) {
    return type == "UT" || type == "IT" || type == "ST" ||
           type == "AT" || type == "TEST";
}

bool repository_clean(const std::filesystem::path& project) {
#ifdef _WIN32
    const auto temporary = project / ".captainslog_git_status.tmp";
    const auto command = "git -C \"" + project.string() +
        "\" status --porcelain > \"" + temporary.string() + "\" 2>nul";
#else
    const auto temporary = project / ".captainslog_git_status.tmp";
    const auto command = "git -C \"" + project.string() +
        "\" status --porcelain > \"" + temporary.string() + "\" 2>/dev/null";
#endif
    const int result = std::system(command.c_str());
    std::ifstream input(temporary);
    const std::string content{std::istreambuf_iterator<char>{input}, {}};
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    return result == 0 && trim(content).empty();
}

capcom::yaml::Item find_item(
    const std::filesystem::path& project,
    const std::string& raw_uid,
    std::map<std::string, capcom::yaml::Item>& items) {
    const auto uid = capcom::yaml::normalize_uid(raw_uid);
    items = capcom::yaml::YamlStore{project}.load_all();
    const auto iterator = items.find(uid);
    if (iterator == items.end()) throw std::runtime_error("Item not found: " + uid);
    return iterator->second;
}

std::vector<std::string> readiness_errors(
    const capcom::yaml::Item& item,
    const std::map<std::string, capcom::yaml::Item>& items) {
    capcom::yaml::YamlStore unused{item.file.parent_path().parent_path()};
    (void)unused;
    std::vector<std::string> errors;
    if (item.title.empty()) errors.push_back("title is empty");
    if (item.text.empty()) errors.push_back("text is empty");
    if (item.rationale.empty()) errors.push_back("rationale is empty");
    if (technical_type(item.type) && !test_type(item.type) && item.implementations.empty()) {
        errors.push_back("implementation evidence is missing; run cap scan");
    }
    if (test_type(item.type) &&
        item.test.status != "Passed" && item.test.status != "Tested") {
        errors.push_back("test result is not Passed");
    }
    for (const auto& parent : item.parents) {
        if (!items.contains(parent)) errors.push_back("missing parent " + parent);
    }
    for (const auto& child : item.children) {
        if (!items.contains(child)) errors.push_back("missing child " + child);
    }

    if (item.type == "SRS" || item.type == "SEC") {
        bool has_verification_test = false;
        for (const auto& child_uid : item.children) {
            const auto child = items.find(child_uid);
            if (child == items.end()) continue;
            const auto& type = child->second.type;
            if (!(type == "TEST" || type == "UT" || type == "IT" ||
                  type == "ST" || type == "AT")) continue;
            has_verification_test = true;
            if (child->second.test.status != "Passed") {
                errors.push_back("verification test " + child_uid + " is not Passed");
            }
        }
        if (!has_verification_test) {
            errors.push_back("no linked verification test child");
        }
    }
    return errors;
}

void print_check(
    const capcom::yaml::Item& item,
    const std::map<std::string, capcom::yaml::Item>& items,
    const capcom::identity::Identity& identity,
    const bool clean) {
    const auto errors = readiness_errors(item, items);
    std::cout << "Review check for " << item.uid << "\n\n"
              << (item.status == "In Review" ? "[OK] " : "[--] ")
              << "Status: " << item.status << '\n'
              << (errors.empty() ? "[OK] " : "[FAIL] ")
              << "Requirement completeness\n"
              << (clean ? "[OK] " : "[WARN] ")
              << "Git working tree " << (clean ? "clean" : "contains changes") << '\n';
    for (const auto& error : errors) std::cout << "       - " << error << '\n';
    std::cout << "[INFO] Current reviewer key: " << identity.key_id << "\n\n"
              << "Manual review required:\n"
              << "  [ ] Requirement is clear, unambiguous and testable\n"
              << "  [ ] Traceability links are complete and correct\n"
              << "  [ ] Implementation satisfies the requirement\n"
              << "  [ ] Error and boundary cases are covered\n"
              << "  [ ] Test procedure and result are suitable\n";
}

} // namespace

int ReviewCommand::execute(
    const std::filesystem::path& project,
    const std::string& raw_uid,
    const ReviewAction action,
    const std::string& reason) const {
    std::map<std::string, capcom::yaml::Item> items;
    const auto item = find_item(project, raw_uid, items);
    const auto identity = capcom::identity::IdentityManager{}.load_or_create();
    capcom::audit::HistoryService history{identity};

    history.verify_project(project);
    const auto validation_errors = capcom::yaml::YamlStore{project}.validate(items);
    const auto readiness = readiness_errors(item, items);

    if (action == ReviewAction::check) {
        print_check(item, items, identity, repository_clean(project));
        return readiness.empty() && validation_errors.empty() ? 0 : 2;
    }

    if (reason.empty()) throw std::runtime_error("A review reason is required with -m.");

    if (action == ReviewAction::submit) {
        if (item.status == "Approved") {
            throw std::runtime_error("Approved items must be changed before a new review.");
        }
        if (!validation_errors.empty()) {
            throw std::runtime_error("Project validation failed. Run 'cap validate'.");
        }
        if (!readiness.empty()) {
            throw std::runtime_error("Item is not review-ready. Run 'cap review-check " + item.uid + "'.");
        }
        replace_status(item.file, "In Review");
        history.append(item.file, item.uid, "SUBMITTED_FOR_REVIEW", reason);
        history.verify_file(item.file, item.uid);
        std::cout << item.uid << " submitted for review.\n";
        return 0;
    }

    if (item.status != "In Review") {
        throw std::runtime_error("Item must be In Review before approve or reject.");
    }

    const auto author_key = history.first_key_for_action(item.file, item.uid, "ITEM_CREATED");
    const auto submitter_key = history.latest_key_for_action(item.file, item.uid, "SUBMITTED_FOR_REVIEW");
    if (identity.key_id == author_key || identity.key_id == submitter_key) {
        throw std::runtime_error(
            "FOUR-EYES VIOLATION: Author/submitter cannot approve or reject " + item.uid + ".");
    }

    if (action == ReviewAction::approve) {
        if (!repository_clean(project)) {
            throw std::runtime_error("Git working tree is not clean. Commit the reviewed state first.");
        }
        if (!validation_errors.empty() || !readiness.empty()) {
            throw std::runtime_error("Item is not approvable. Run 'cap review-check " + item.uid + "'.");
        }
        replace_status(item.file, "Approved");
        history.append(item.file, item.uid, "APPROVED", reason);
        history.verify_file(item.file, item.uid);
        std::cout << item.uid << " approved by " << identity.full_name << ".\n";
        return 0;
    }

    replace_status(item.file, "Rejected");
    history.append(item.file, item.uid, "REJECTED", reason);
    history.verify_file(item.file, item.uid);
    std::cout << item.uid << " rejected by " << identity.full_name << ".\n";
    return 0;
}

} // namespace capcom::commands


