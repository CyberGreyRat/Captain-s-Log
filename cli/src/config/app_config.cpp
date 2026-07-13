#include "capcom/config/app_config.hpp"

namespace capcom::config {

AppConfig ConfigLoader::load(const std::filesystem::path& project_directory) const {
    // The final lookup order (.capcom_config.json / environment) will be
    // implemented after review.
    (void)project_directory;
    return {};
}

} // namespace capcom::config
