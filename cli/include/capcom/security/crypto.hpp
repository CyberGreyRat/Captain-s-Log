#pragma once
#include <string>
#include <string_view>
namespace capcom::security {
[[nodiscard]] std::string sha256_hex(std::string_view data);
[[nodiscard]] std::string sign_rsa_pss_sha256(
    const std::string& key_name, std::string_view payload);
[[nodiscard]] bool verify_rsa_pss_sha256(
    const std::string& public_key_base64,
    std::string_view payload,
    const std::string& signature_base64);
[[nodiscard]] std::string random_hex(std::size_t bytes);
}
