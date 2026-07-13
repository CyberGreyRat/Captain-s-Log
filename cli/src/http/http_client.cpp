#include "capcom/http/http_client.hpp"

#include <utility>

namespace capcom::http {

HttpClient::HttpClient(HttpClientOptions options)
    : options_{std::move(options)} {}

HttpResponse HttpClient::post_json(
    const std::string_view route,
    const std::string_view json_body) const {
    // libcurl lifecycle, headers, timeout handling and error mapping are
    // intentionally implemented in the next isolated step.
    (void)route;
    (void)json_body;
    return {};
}

} // namespace capcom::http
