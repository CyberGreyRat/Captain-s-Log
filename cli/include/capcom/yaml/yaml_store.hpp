#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>
namespace capcom::yaml {
struct Implementation { std::string file; int start_line{}; int end_line{}; std::string git_hash; };
struct TestResult { std::string status; std::string source; std::string timestamp; std::string message; };
struct Item {
 std::string uid,type,status,title,text,rationale,version,git_hash;
 std::map<std::string,std::string> attributes;
 std::vector<std::string> parents,children;
 std::vector<Implementation> implementations;
 TestResult test;
 std::filesystem::path file;
};
class YamlStore final {
public:
 explicit YamlStore(std::filesystem::path project);
 [[nodiscard]] std::map<std::string,Item> load_all() const;
 void replace_implementations(const std::map<std::string,std::vector<Implementation>>& values) const;
 void update_test(const std::string& uid,const TestResult& result) const;
 [[nodiscard]] std::vector<std::string> validate(const std::map<std::string,Item>& items) const;
private: std::filesystem::path project_;
};
[[nodiscard]] std::string normalize_uid(std::string value);
}


