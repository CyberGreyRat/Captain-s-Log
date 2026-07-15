#include "capcom/commands/gui_command.hpp"
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
namespace capcom::commands {namespace {
std::string quote(const std::filesystem::path&p){std::string r{"\""};for(char c:p.string()){if(c=='"')r+="\\\"";else r+=c;}return r+'"';}
std::filesystem::path root(){if(const char*v=std::getenv("CAPTAIN_WEB_ROOT")){if(*v)return v;}if(const char*v=std::getenv("USERPROFILE"))return std::filesystem::path{v}/"Documents"/"CapCom"/"captain-web";throw std::runtime_error("Set CAPTAIN_WEB_ROOT to the captain-web directory.");}
int port(const std::vector<std::string>&a){int p=8080;for(std::size_t i=0;i<a.size();++i){if(a[i]!="--port"||i+1>=a.size())throw std::runtime_error("Usage: cap gui [--port <port>]");p=std::stoi(a[++i]);}if(p<1024||p>65535)throw std::runtime_error("Port must be between 1024 and 65535.");return p;}
}
int GuiCommand::execute(const std::filesystem::path&project,const std::vector<std::string>&arguments)const{if(!std::filesystem::exists(project/"reqs"))throw std::runtime_error("Current directory is not an initialized Captain project. Run 'cap init'.");const auto web=root();const auto launcher=web/"cap-gui.ps1";const auto server=web/"build"/"debug"/"captain-web.exe";if(!std::filesystem::exists(launcher))throw std::runtime_error("GUI launcher not found: "+launcher.string());if(!std::filesystem::exists(server))throw std::runtime_error("Web server not built: "+server.string());std::ostringstream command;command<<"powershell.exe -NoProfile -ExecutionPolicy Bypass -File "<<quote(launcher)<<" -Port "<<port(arguments);std::cout<<"Starting Captain's Log for "<<project.string()<<'\n';const int result=std::system(command.str().c_str());if(result!=0)throw std::runtime_error("Captain GUI launcher failed.");return 0;}}
