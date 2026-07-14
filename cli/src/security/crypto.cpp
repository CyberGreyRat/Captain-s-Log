#include "capcom/security/crypto.hpp"
#include <stdexcept>
#include <vector>
#include <sstream>
#include <iomanip>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#include <ncrypt.h>
#include <wincrypt.h>
#endif
namespace capcom::security {
#ifdef _WIN32
namespace {
void check_nt(NTSTATUS status, const char* operation) {
    if (status < 0) throw std::runtime_error(std::string(operation) + " failed");
}
void check_sec(SECURITY_STATUS status, const char* operation) {
    if (status != ERROR_SUCCESS) throw std::runtime_error(std::string(operation) + " failed");
}
std::wstring widen(const std::string& value) {
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(),
        static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        result.data(), size);
    return result;
}
std::vector<unsigned char> hash_sha256(std::string_view data) {
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    check_nt(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM,
        nullptr, 0), "BCryptOpenAlgorithmProvider");
    DWORD object_size{}, result_size{};
    check_nt(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size),
        &result_size, 0), "BCryptGetProperty");
    std::vector<unsigned char> object(object_size);
    std::vector<unsigned char> digest(32);
    check_nt(BCryptCreateHash(algorithm, &hash, object.data(), object_size,
        nullptr, 0, 0), "BCryptCreateHash");
    check_nt(BCryptHashData(hash,
        reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())),
        static_cast<ULONG>(data.size()), 0), "BCryptHashData");
    check_nt(BCryptFinishHash(hash, digest.data(),
        static_cast<ULONG>(digest.size()), 0), "BCryptFinishHash");
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return digest;
}
std::string base64_encode(const std::vector<unsigned char>& bytes) {
    DWORD size{};
    if (!CryptBinaryToStringA(bytes.data(), static_cast<DWORD>(bytes.size()),
        CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &size)) {
        throw std::runtime_error("Base64 encoding failed");
    }
    std::string result(size, '\0');
    if (!CryptBinaryToStringA(bytes.data(), static_cast<DWORD>(bytes.size()),
        CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, result.data(), &size)) {
        throw std::runtime_error("Base64 encoding failed");
    }
    if (!result.empty() && result.back() == '\0') result.pop_back();
    return result;
}
std::vector<unsigned char> base64_decode(const std::string& text) {
    DWORD size{};
    if (!CryptStringToBinaryA(text.c_str(), 0, CRYPT_STRING_BASE64,
        nullptr, &size, nullptr, nullptr)) {
        throw std::runtime_error("Base64 decoding failed");
    }
    std::vector<unsigned char> result(size);
    if (!CryptStringToBinaryA(text.c_str(), 0, CRYPT_STRING_BASE64,
        result.data(), &size, nullptr, nullptr)) {
        throw std::runtime_error("Base64 decoding failed");
    }
    result.resize(size);
    return result;
}
}
std::string sha256_hex(std::string_view data) {
    const auto digest = hash_sha256(data);
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto byte : digest) output << std::setw(2) << static_cast<int>(byte);
    return output.str();
}
std::string random_hex(std::size_t bytes) {
    std::vector<unsigned char> value(bytes);
    check_nt(BCryptGenRandom(nullptr, value.data(), static_cast<ULONG>(value.size()),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG), "BCryptGenRandom");
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto byte : value) output << std::setw(2) << static_cast<int>(byte);
    return output.str();
}
std::string sign_rsa_pss_sha256(const std::string& key_name,
                                std::string_view payload) {
    NCRYPT_PROV_HANDLE provider{};
    NCRYPT_KEY_HANDLE key{};
    check_sec(NCryptOpenStorageProvider(&provider,
        MS_KEY_STORAGE_PROVIDER, 0), "NCryptOpenStorageProvider");
    const auto wide_name = widen(key_name);
    check_sec(NCryptOpenKey(provider, &key, wide_name.c_str(), 0, 0),
        "NCryptOpenKey");
    const auto digest = hash_sha256(payload);
    BCRYPT_PSS_PADDING_INFO padding{BCRYPT_SHA256_ALGORITHM, 32};
    DWORD size{};
    check_sec(NCryptSignHash(key, &padding,
        const_cast<PBYTE>(digest.data()), static_cast<DWORD>(digest.size()),
        nullptr, 0, &size, NCRYPT_PAD_PSS_FLAG), "NCryptSignHash");
    std::vector<unsigned char> signature(size);
    check_sec(NCryptSignHash(key, &padding,
        const_cast<PBYTE>(digest.data()), static_cast<DWORD>(digest.size()),
        signature.data(), size, &size, NCRYPT_PAD_PSS_FLAG), "NCryptSignHash");
    signature.resize(size);
    NCryptFreeObject(key);
    NCryptFreeObject(provider);
    return base64_encode(signature);
}
bool verify_rsa_pss_sha256(const std::string& public_key_base64,
                           std::string_view payload,
                           const std::string& signature_base64) {
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_KEY_HANDLE key{};
    check_nt(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_RSA_ALGORITHM,
        nullptr, 0), "BCryptOpenAlgorithmProvider");
    auto public_blob = base64_decode(public_key_base64);
    check_nt(BCryptImportKeyPair(algorithm, nullptr, BCRYPT_RSAPUBLIC_BLOB,
        &key, public_blob.data(), static_cast<ULONG>(public_blob.size()), 0),
        "BCryptImportKeyPair");
    const auto digest = hash_sha256(payload);
    auto signature = base64_decode(signature_base64);
    BCRYPT_PSS_PADDING_INFO padding{BCRYPT_SHA256_ALGORITHM, 32};
    const auto status = BCryptVerifySignature(key, &padding,
        const_cast<PUCHAR>(digest.data()), static_cast<ULONG>(digest.size()),
        signature.data(), static_cast<ULONG>(signature.size()),
        BCRYPT_PAD_PSS);
    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return status >= 0;
}
#else
std::string sha256_hex(std::string_view) { throw std::runtime_error("Windows CNG required"); }
std::string random_hex(std::size_t) { throw std::runtime_error("Windows CNG required"); }
std::string sign_rsa_pss_sha256(const std::string&, std::string_view) { throw std::runtime_error("Windows CNG required"); }
bool verify_rsa_pss_sha256(const std::string&, std::string_view, const std::string&) { return false; }
#endif
}
