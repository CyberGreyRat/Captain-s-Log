#include "captain/api/cli_bridge.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif
namespace captain::api {namespace {
std::string quote(const std::filesystem::path& path) {
    std::string value = path.string();
    std::string result{"\""};
    for (const char character : value) {
        if (character == '"') result += "\\\"";
        else result.push_back(character);
    }
    result.push_back('"');
    return result;
}
}
CliBridge::CliBridge(std::filesystem::path project,std::filesystem::path executable)
    :project_(std::move(project)),executable_(std::move(executable)){}
CommandResult CliBridge::run(const std::string& arguments) const {
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(project_);
    const std::string command = quote(executable_) + " " + arguments + " 2>&1";
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (!pipe) {
        std::filesystem::current_path(previous);
        throw std::runtime_error("Cannot start Captain CLI.");
    }
    std::array<char,4096> buffer{};
    std::string output;
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
        output += buffer.data();
    }
#ifdef _WIN32
    const int exit_code = _pclose(pipe);
#else
    const int exit_code = pclose(pipe);
#endif
    std::filesystem::current_path(previous);
    return {exit_code, output};
}
CommandResult CliBridge::scan()const{return run("scan");}
CommandResult CliBridge::verify()const{return run("verify");}
CommandResult CliBridge::validate()const{return run("validate");}
CommandResult CliBridge::status()const{return run("status");}
CommandResult CliBridge::tree()const{return run("tree");}
CommandResult CliBridge::publish()const{return run("publish");}
}
