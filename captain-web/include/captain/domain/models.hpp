#pragma once
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
namespace captain::domain {
enum class Authority { local, remote, mirrored };
struct Requirement {
    std::string uid, type, status{"Draft"}, title, text, rationale;
    Authority authority{Authority::local}; bool read_only{};
    std::vector<std::string> parents, children;
    std::map<std::string,std::string> attributes;
    std::string revision, updated_at, updated_by;
};
struct CreateRequirement {std::string type,title,text,rationale;std::vector<std::string> parents,children;};
struct Comment {std::string id,requirement_uid,author,created_at,text;};
struct FileLink {std::string id,requirement_uid,kind,label,location,version,hash,created_at,created_by;};
inline std::string authority_name(Authority a){return a==Authority::local?"local":a==Authority::remote?"remote":"mirrored";}
inline void to_json(nlohmann::json& j,const Requirement& r){j={{"uid",r.uid},{"type",r.type},{"status",r.status},{"title",r.title},{"text",r.text},{"rationale",r.rationale},{"authority",authority_name(r.authority)},{"read_only",r.read_only},{"parents",r.parents},{"children",r.children},{"attributes",r.attributes},{"revision",r.revision},{"updated_at",r.updated_at},{"updated_by",r.updated_by}};}
inline void to_json(nlohmann::json& j,const Comment& v){j={{"id",v.id},{"requirement_uid",v.requirement_uid},{"author",v.author},{"created_at",v.created_at},{"text",v.text}};}
inline void to_json(nlohmann::json& j,const FileLink& v){j={{"id",v.id},{"requirement_uid",v.requirement_uid},{"kind",v.kind},{"label",v.label},{"location",v.location},{"version",v.version},{"hash",v.hash},{"created_at",v.created_at},{"created_by",v.created_by}};}
}
