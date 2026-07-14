#pragma once
#include <chrono>
#include <string>
#include <string_view>
namespace capcom::http {
struct HttpResponse { long status_code{}; std::string body; };
struct HttpClientOptions { std::string base_url{"http://127.0.0.1:8000"}; std::string api_key; std::chrono::milliseconds timeout{10000}; };
class HttpClient final { public: explicit HttpClient(HttpClientOptions options); [[nodiscard]] HttpResponse post_json(std::string_view route, std::string_view json_body) const; private: HttpClientOptions options_; };
}


