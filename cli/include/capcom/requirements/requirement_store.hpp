#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>
namespace capcom::requirements {
struct Requirement { std::string uid, type, title, status; std::vector<std::string> parents, children; std::filesystem::path file; };
class RequirementStore final {
public:
 explicit RequirementStore(std::filesystem::path project);
 [[nodiscard]] std::map<std::string, Requirement> load_all() const;
 void add_link(const std::string& child, const std::string& parent) const;
 void remove_link(const std::string& child, const std::string& parent) const;
private: std::filesystem::path project_;
};
}
