#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
namespace captain::document {
struct DocumentInfo {
    std::string id, original_name, stored_name, mime_type, sha256;
    std::uint64_t size{};
    int revision{1};
    std::string uploaded_by, uploaded_at;
};
inline void to_json(nlohmann::json& j, const DocumentInfo& d) {
    j={{"id",d.id},{"original_name",d.original_name},{"mime_type",d.mime_type},
       {"sha256",d.sha256},{"size",d.size},{"revision",d.revision},
       {"uploaded_by",d.uploaded_by},{"uploaded_at",d.uploaded_at}};
}
class DocumentService final {
public:
    explicit DocumentService(std::filesystem::path project);
    DocumentInfo upload(const std::string& filename,const std::string& content,
                        const std::string& mime_type,const std::string& actor);
    std::vector<DocumentInfo> all() const;
    std::filesystem::path content_path(const std::string& document_id) const;
    void link(const std::string& requirement_uid,const std::string& document_id,
              const std::string& label,const std::string& fragment);
private:
    void initialize();
    std::filesystem::path root_, index_, links_;
};
}
