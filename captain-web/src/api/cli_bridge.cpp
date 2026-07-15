#include "captain/api/cli_bridge.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>
namespace captain::api {namespace {std::string quote(const std::string&s){std::string o="\"";for(char c:s){if(c=='"')o+="\\\"";else o+=c;}return o+'"';}}
CliBridge::CliBridge(std::filesystem::path p,std::filesystem::path e):project_(std::move(p)),executable_(std::move(e)){}CommandResult CliBridge::run(const std::string&a)const{auto old=std::filesystem::current_path();std::filesystem::current_path(project_);auto command=quote(executable_.string())+" "+a+" 2>&1";
#ifdef _WIN32
 FILE*pipe=_popen(command.c_str(),"r");
#else
 FILE*pipe=popen(command.c_str(),"r");
#endif
 if(!pipe){std::filesystem::current_path(old);throw std::runtime_error("Cannot start Captain CLI");}std::array<char,4096>b{};std::string out;while(std::fgets(b.data(),static_cast<int>(b.size()),pipe))out+=b.data();
#ifdef _WIN32
 int code=_pclose(pipe);
#else
 int code=pclose(pipe);
#endif
 std::filesystem::current_path(old);return{code,out};}
CommandResult CliBridge::scan()const{return run("scan");}CommandResult CliBridge::verify()const{return run("verify");}CommandResult CliBridge::validate()const{return run("validate");}CommandResult CliBridge::status()const{return run("status");}CommandResult CliBridge::tree()const{return run("tree");}CommandResult CliBridge::publish()const{return run("publish");}CommandResult CliBridge::sign(const std::string&u,const std::string&m)const{return run("sign "+u+" -m "+quote(m));}}
