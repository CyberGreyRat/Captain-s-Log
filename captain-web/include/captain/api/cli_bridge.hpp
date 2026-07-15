#pragma once
#include <filesystem>
#include <string>
namespace captain::api {
struct CommandResult { int exit_code{}; std::string output; };
class CliBridge final {
public:
    CliBridge(std::filesystem::path project, std::filesystem::path executable);
    [[nodiscard]] CommandResult scan() const;
    [[nodiscard]] CommandResult verify() const;
    [[nodiscard]] CommandResult validate() const;
    [[nodiscard]] CommandResult status() const;
    [[nodiscard]] CommandResult tree() const;
    [[nodiscard]] CommandResult publish() const;
private:
    [[nodiscard]] CommandResult run(const std::string& arguments) const;
    std::filesystem::path project_;
    std::filesystem::path executable_;
};
}
