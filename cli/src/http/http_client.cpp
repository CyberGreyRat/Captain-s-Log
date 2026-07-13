#include "capcom/http/http_client.hpp"
#include <utility>
namespace capcom::http {
HttpClient::HttpClient(HttpClientOptions options) : options_{std::move(options)} {}
HttpResponse HttpClient::post_json(std::string_view route, std::string_view json_body) const { (void)route; (void)json_body; return {}; }
}
