#include "capcom/identity/identity_manager.hpp"
#include "capcom/security/crypto.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#include <ncrypt.h>
#include <wincrypt.h>
#endif
namespace capcom::identity {
namespace {
std::filesystem::path identity_path() {
    const char* profile = std::getenv("USERPROFILE");
    if (profile == nullptr || *profile == '\0') throw std::runtime_error("USERPROFILE is unavailable");
    return std::filesystem::path{profile} / ".captainslog_identity.json";
}
std::string escape_json(const std::string& value) {
    std::string out;
    for (char c : value) { if (c == '\\' || c == '"') out.push_back('\\'); out.push_back(c); }
    return out;
}
std::string field(const std::string& json, const std::string& name) {
    std::smatch match;
    const std::regex pattern{"\"" + name + "\"\\s*:\\s*\"([^\"]*)\""};
    if (!std::regex_search(json, match, pattern)) throw std::runtime_error("Invalid identity file: missing " + name);
    return match[1].str();
}
#ifdef _WIN32
std::string encode(const std::vector<unsigned char>& bytes) {
    DWORD size{};
    CryptBinaryToStringA(bytes.data(), static_cast<DWORD>(bytes.size()),
        CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &size);
    std::string result(size, '\0');
    if (!CryptBinaryToStringA(bytes.data(), static_cast<DWORD>(bytes.size()),
        CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, result.data(), &size)) {
        throw std::runtime_error("Public key encoding failed");
    }
    if (!result.empty() && result.back() == '\0') result.pop_back();
    return result;
}
Identity create() {
    std::cout << "Captain's Log identity setup\n"
              << "Bitte gib deinen vollen Namen fuer den Audit-Trail ein: " << std::flush;
    std::string name;
    std::getline(std::cin, name);
    if (name.empty()) throw std::runtime_error("A full name is required");
    const auto token = capcom::security::random_hex(16);
    const auto key_name = "CaptainsLog-Audit-" + token;
    NCRYPT_PROV_HANDLE provider{};
    NCRYPT_KEY_HANDLE key{};
    if (NCryptOpenStorageProvider(&provider, MS_KEY_STORAGE_PROVIDER, 0) != ERROR_SUCCESS)
        throw std::runtime_error("Cannot open Windows key provider");
    std::wstring wide_key(key_name.begin(), key_name.end());
    if (NCryptCreatePersistedKey(provider, &key, NCRYPT_RSA_ALGORITHM,
        wide_key.c_str(), 0, NCRYPT_OVERWRITE_KEY_FLAG) != ERROR_SUCCESS)
        throw std::runtime_error("Cannot create audit key");
    DWORD length = 2048;
    NCryptSetProperty(key, NCRYPT_LENGTH_PROPERTY,
        reinterpret_cast<PBYTE>(&length), sizeof(length), 0);
    DWORD export_policy = 0;
    NCryptSetProperty(key, NCRYPT_EXPORT_POLICY_PROPERTY,
        reinterpret_cast<PBYTE>(&export_policy), sizeof(export_policy), 0);
    if (NCryptFinalizeKey(key, 0) != ERROR_SUCCESS)
        throw std::runtime_error("Cannot finalize audit key");
    DWORD blob_size{};
    NCryptExportKey(key, 0, BCRYPT_RSAPUBLIC_BLOB, nullptr,
        nullptr, 0, &blob_size, 0);
    std::vector<unsigned char> blob(blob_size);
    if (NCryptExportKey(key, 0, BCRYPT_RSAPUBLIC_BLOB, nullptr,
        blob.data(), blob_size, &blob_size, 0) != ERROR_SUCCESS)
        throw std::runtime_error("Cannot export public key");
    blob.resize(blob_size);
    NCryptFreeObject(key);
    NCryptFreeObject(provider);
    char computer[MAX_COMPUTERNAME_LENGTH + 1]{};
    DWORD computer_size = sizeof(computer);
    GetComputerNameA(computer, &computer_size);
    Identity identity{name, "HOST-" + capcom::security::sha256_hex(
        std::string{computer} + token).substr(0, 20), key_name,
        "KEY-" + capcom::security::sha256_hex(encode(blob)).substr(0, 20),
        "RSA-PSS-SHA256", encode(blob)};
    std::ofstream output(identity_path(), std::ios::trunc);
    output << "{\n"
           << "  \"schema_version\": 1,\n"
           << "  \"full_name\": \"" << escape_json(identity.full_name) << "\",\n"
           << "  \"host_id\": \"" << identity.host_id << "\",\n"
           << "  \"key_name\": \"" << identity.key_name << "\",\n"
           << "  \"key_id\": \"" << identity.key_id << "\",\n"
           << "  \"algorithm\": \"" << identity.algorithm << "\",\n"
           << "  \"public_key\": \"" << identity.public_key_base64 << "\"\n}\n";
    std::filesystem::permissions(identity_path(),
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace);
    return identity;
}
#endif
}
Identity IdentityManager::load_or_create() const {
    const auto path = identity_path();
    if (!std::filesystem::exists(path)) {
#ifdef _WIN32
        return create();
#else
        throw std::runtime_error("Captain's Log cryptographic identity requires Windows");
#endif
    }
    std::ifstream input(path);
    const std::string json{std::istreambuf_iterator<char>{input}, {}};
    return {field(json, "full_name"), field(json, "host_id"),
        field(json, "key_name"), field(json, "key_id"),
        field(json, "algorithm"), field(json, "public_key")};
}
}
