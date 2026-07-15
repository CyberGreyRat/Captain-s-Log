#pragma once
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
namespace captain::domain {
enum class Authority { local, remote, mirrored };
struct Requirement {
    std::string uid,type,status{"Draft"},title,text,rationale;
    Authority authority{Authority::local}; bool read_only{};
    std::vector<std::string> parents,children;
    std::map<std::string,std::string> attributes;
    std::string revision,updated_at,updated_by;
    std::string created_by,created_host,created_at,created_source;
    std::string sync_state{"local_only"},synced_at,synced_by,synced_revision;
};
struct CreateRequirement {std::string type,title,text,rationale;std::vector<std::string> parents,children;std::string actor,host,source{"Captain Web"};};
struct Comment {std::string id,requirement_uid,author,created_at,text;};
struct FileLink {std::string id,requirement_uid,kind,label,location,version,hash,created_at,created_by;bool global{};};
inline std::string authority_name(Authority a){return a==Authority::local?"local":a==Authority::remote?"remote":"mirrored";}
inline void to_json(nlohmann::json&j,const Requirement&r){j={{"uid",r.uid},{"type",r.type},{"status",r.status},{"title",r.title},{"text",r.text},{"rationale",r.rationale},{"authority",authority_name(r.authority)},{"read_only",r.read_only},{"parents",r.parents},{"children",r.children},{"attributes",r.attributes},{"revision",r.revision},{"updated_at",r.updated_at},{"updated_by",r.updated_by},{"created_by",r.created_by},{"created_host",r.created_host},{"created_at",r.created_at},{"created_source",r.created_source},{"sync_state",r.sync_state},{"synced_at",r.synced_at},{"synced_by",r.synced_by},{"synced_revision",r.synced_revision}};}
inline void to_json(nlohmann::json&j,const Comment&v){j={{"id",v.id},{"requirement_uid",v.requirement_uid},{"author",v.author},{"created_at",v.created_at},{"text",v.text}};}
inline void to_json(nlohmann::json&j,const FileLink&v){j={{"id",v.id},{"requirement_uid",v.requirement_uid},{"kind",v.kind},{"label",v.label},{"location",v.location},{"version",v.version},{"hash",v.hash},{"created_at",v.created_at},{"created_by",v.created_by},{"global",v.global}};}
}
