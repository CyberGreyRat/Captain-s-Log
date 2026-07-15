#include "captain/auth/auth_service.hpp"
#include <chrono>
#include <random>
#include <sstream>
namespace captain::auth {namespace {
std::string random_id(){std::random_device rd;std::mt19937_64 gen(rd());std::ostringstream out;out<<std::hex<<gen()<<gen();return out.str();}
}
AuthService::AuthService(std::string ticket,std::string user,std::string host,std::string project_id):ticket_(std::move(ticket)),user_(std::move(user)),host_(std::move(host)),project_id_(std::move(project_id)){}
std::optional<Session> AuthService::exchange_ticket(const std::string& ticket){std::scoped_lock lock(mutex_);if(ticket_.empty()||ticket!=ticket_)return std::nullopt;ticket_.clear();Session value{random_id(),user_,host_,"Developer",project_id_,std::chrono::system_clock::now()+std::chrono::minutes(60)};sessions_[value.id]=value;return value;}
std::string AuthService::cookie_value(const std::string& header,const std::string& name){const auto key=name+"=";auto start=header.find(key);if(start==std::string::npos)return{};start+=key.size();auto end=header.find(';',start);return header.substr(start,end==std::string::npos?std::string::npos:end-start);}
std::optional<Session> AuthService::session(const std::string& header)const{const auto id=cookie_value(header,"captain_session");if(id.empty())return std::nullopt;std::scoped_lock lock(mutex_);const auto found=sessions_.find(id);if(found==sessions_.end())return std::nullopt;if(found->second.expires_at<std::chrono::system_clock::now()){sessions_.erase(found);return std::nullopt;}return found->second;}
}
