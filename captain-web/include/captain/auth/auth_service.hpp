#pragma once
#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <string>
namespace captain::auth {
struct Session {std::string id,user,host,role,project_id;std::chrono::system_clock::time_point expires_at;};
class AuthService final {
public:
    AuthService(std::string startup_ticket,std::string user,std::string host,std::string project_id);
    std::optional<Session> exchange_ticket(const std::string& ticket);
    std::optional<Session> session(const std::string& cookie_header) const;
    static std::string cookie_value(const std::string& cookie_header,const std::string& name);
private:
    std::string ticket_,user_,host_,project_id_;
    mutable std::mutex mutex_;
    mutable std::map<std::string,Session> sessions_;
};
}
