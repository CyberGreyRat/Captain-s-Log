#pragma once
#include <string>
namespace capcom::identity {
struct Identity {
    std::string full_name;
    std::string host_id;
    std::string key_name;
    std::string key_id;
    std::string algorithm;
    std::string public_key_base64;
};
class IdentityManager final {
public:
    [[nodiscard]] Identity load_or_create() const;
};
}


