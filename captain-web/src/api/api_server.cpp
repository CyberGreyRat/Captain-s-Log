#include "captain/api/api_server.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>
namespace captain::api {namespace {
using json=nlohmann::json;
void reply(httplib::Response&r,const json&j,int status=200){r.status=status;r.set_content(j.dump(),"application/json; charset=utf-8");}
void fail(httplib::Response&r,int status,const std::string&m){reply(r,{{"error",{{"message",m}}}},status);}
std::string read(const std::filesystem::path&p){std::ifstream in(p,std::ios::binary);if(!in)return{};return{std::istreambuf_iterator<char>(in),{}};}
std::string now(){auto t=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());std::tm x{};
#ifdef _WIN32
 gmtime_s(&x,&t);
#else
 gmtime_r(&t,&x);
#endif
 std::ostringstream o;o<<std::put_time(&x,"%Y-%m-%dT%H:%M:%SZ");return o.str();}
json events(const std::filesystem::path&p){std::ifstream in(p);if(!in)return json::array();try{return json::parse(in);}catch(...){return json::array();}}
void save_events(const std::filesystem::path&p,const json&j){std::filesystem::create_directories(p.parent_path());auto tmp=p;tmp+=".tmp";std::ofstream out(tmp,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("Cannot write audit history");out<<j.dump(2);out.close();std::error_code ec;std::filesystem::remove(p,ec);ec.clear();std::filesystem::rename(tmp,p,ec);if(ec)throw std::runtime_error("Cannot replace audit history");}
std::vector<std::string> list(const json&j,const char*k){return j.contains(k)?j.at(k).get<std::vector<std::string>>():std::vector<std::string>{};}
json command(const CommandResult&r){return{{"ok",r.exit_code==0},{"exit_code",r.exit_code},{"output",r.output}};}
}
ApiServer::ApiServer(repository::HybridRepository&r,sync::SyncService&s,project::ProjectCatalog&c,auth::AuthService&a,std::string id,std::string name,std::filesystem::path project_path,std::filesystem::path web,std::filesystem::path cap):repository_(r),sync_(s),catalog_(c),auth_(a),project_id_(std::move(id)),project_name_(std::move(name)),project_(std::move(project_path)),web_(std::move(web)),cli_(project_,std::move(cap)){}
std::optional<auth::Session>ApiServer::developer(const httplib::Request&q)const{return auth_.session(q.get_header_value("Cookie"));}
bool ApiServer::require_developer(const httplib::Request&q,httplib::Response&r)const{if(developer(q))return true;fail(r,403,"Diese Aktion erfordert eine ueber cap gui gestartete Developer-Sitzung.");return false;}
std::string ApiServer::actor(const httplib::Request&q)const{auto s=developer(q);return s?s->user:"viewer";}
std::string ApiServer::actor_host(const httplib::Request&q)const{auto s=developer(q);return s?s->host:"browser";}
void ApiServer::audit(const std::string&action,const std::string&uid,const std::string&detail,const std::string&user,const std::string&host)const{auto p=project_/".captain"/"web-audit.json";auto j=events(p);j.push_back({{"timestamp",now()},{"action",action},{"object_uid",uid},{"actor",user},{"host",host},{"source","Captain Web"},{"detail",detail}});save_events(p,j);}
void ApiServer::routes(){
 server_.Get("/",[this](const auto&,auto&r){r.set_content(read(web_/"index.html"),"text/html; charset=utf-8");});
 server_.Get("/logo.png",[this](const auto&,auto&r){r.set_content(read(web_/"logo.png"),"image/png");});
 server_.Get("/auth/cap-gui",[this](const auto&q,auto&r){auto s=auth_.exchange_ticket(q.get_param_value("ticket"));if(!s){fail(r,401,"Ungueltiges oder abgelaufenes Ticket.");return;}r.set_header("Set-Cookie","captain_session="+s->id+"; HttpOnly; SameSite=Strict; Path=/; Max-Age=3600");r.set_redirect("/");});
 server_.Get("/api/v1/session",[this](const auto&q,auto&r){auto s=developer(q);reply(r,{{"authenticated",static_cast<bool>(s)},{"role",s?"Developer":"Viewer"},{"can_write",static_cast<bool>(s)},{"user",s?s->user:"Anonymous"},{"host",s?s->host:""},{"active_project",project_id_},{"project_name",project_name_}});});
 server_.Get("/api/v1/projects",[this](const auto&,auto&r){reply(r,{{"active_project",project_id_},{"projects",catalog_.projects()}});});
 server_.Get(R"(/api/v1/projects/([A-Za-z0-9_-]+)/requirements)",[this](const auto&q,auto&r){const auto id=q.matches[1].str();if(id==project_id_)reply(r,{{"items",repository_.all()},{"read_only",false}});else reply(r,{{"items",catalog_.snapshots(id)},{"read_only",true}});});
 server_.Get("/api/v1/requirements",[this](const auto&,auto&r){try{reply(r,{{"items",repository_.all()}});}catch(const std::exception&e){fail(r,500,e.what());}});
 server_.Post("/api/v1/requirements/create",[this](const auto&q,auto&r){if(!require_developer(q,r))return;try{auto j=json::parse(q.body);domain::CreateRequirement x{j.value("type",""),j.value("title",""),j.value("text",""),j.value("rationale",""),list(j,"parents"),list(j,"children"),actor(q),actor_host(q),"Captain Web"};auto item=repository_.create(x);auto sign=cli_.sign(item.uid,"Created by "+actor(q));audit("REQUIREMENT_CREATED",item.uid,"Created",actor(q),actor_host(q));reply(r,{{"item",item},{"audit",command(sign)}},201);}catch(const std::exception&e){fail(r,422,e.what());}});
 server_.Post(R"(/api/v1/requirements/([A-Za-z0-9_-]+)/update)",[this](const auto&q,auto&r){if(!require_developer(q,r))return;try{auto uid=q.matches[1].str();auto items=repository_.all();auto it=std::find_if(items.begin(),items.end(),[&](const auto&x){return x.uid==uid;});if(it==items.end())throw std::runtime_error("UID not found");auto j=json::parse(q.body);it->title=j.value("title",it->title);it->text=j.value("text",it->text);it->rationale=j.value("rationale",it->rationale);it->updated_by=actor(q);auto item=repository_.update(*it);auto sign=cli_.sign(uid,"Edited by "+actor(q));audit("REQUIREMENT_UPDATED",uid,j.value("reason","Edited"),actor(q),actor_host(q));reply(r,{{"item",item},{"audit",command(sign)}});}catch(const std::exception&e){fail(r,422,e.what());}});
 server_.Post(R"(/api/v1/requirements/([A-Za-z0-9_-]+)/(archive|restore))",[this](const auto&q,auto&r){if(!require_developer(q,r))return;try{auto uid=q.matches[1].str(),operation=q.matches[2].str();auto j=json::parse(q.body);auto reason=j.value("reason","");if(reason.empty())throw std::runtime_error("Grund ist erforderlich");auto items=repository_.all();auto it=std::find_if(items.begin(),items.end(),[&](const auto&x){return x.uid==uid;});if(it==items.end())throw std::runtime_error("UID not found");it->status=operation=="archive"?"Archived":"Draft";it->updated_by=actor(q);auto item=repository_.update(*it);audit(operation=="archive"?"REQUIREMENT_ARCHIVED":"REQUIREMENT_RESTORED",uid,reason,actor(q),actor_host(q));reply(r,{{"item",item}});}catch(const std::exception&e){fail(r,422,e.what());}});
 server_.Get(R"(/api/v1/requirements/([A-Za-z0-9_-]+)/comments)",[this](const auto&q,auto&r){reply(r,{{"comments",repository_.comments(q.matches[1].str())}});});
 server_.Post(R"(/api/v1/requirements/([A-Za-z0-9_-]+)/comment)",[this](const auto&q,auto&r){if(!require_developer(q,r))return;auto j=json::parse(q.body);auto uid=q.matches[1].str();auto item=repository_.add_comment(uid,actor(q),j.value("text",""));audit("COMMENT_ADDED",uid,item.text,actor(q),actor_host(q));reply(r,{{"comment",item}},201);});
 server_.Get(R"(/api/v1/requirements/([A-Za-z0-9_-]+)/files)",[this](const auto&q,auto&r){reply(r,{{"file_links",repository_.files(q.matches[1].str())}});});
 server_.Get("/api/v1/documents",[this](const auto&,auto&r){reply(r,{{"documents",repository_.all_files()}});});
 server_.Post("/api/v1/documents/link",[this](const auto&q,auto&r){if(!require_developer(q,r))return;try{auto j=json::parse(q.body);domain::FileLink f;f.requirement_uid=j.value("requirement_uid","");f.kind=j.value("kind","web_url");f.label=j.value("label","");f.location=j.value("location","");f.version=j.value("version","");f.created_by=actor(q);f.global=f.requirement_uid.empty();if(f.label.empty()||f.location.empty())throw std::runtime_error("Bezeichnung und Adresse erforderlich");auto item=repository_.add_file(f);audit("DOCUMENT_LINKED",f.requirement_uid,f.label,actor(q),actor_host(q));reply(r,{{"document",item}},201);}catch(const std::exception&e){fail(r,422,e.what());}});
 server_.Post(R"(/api/v1/documents/([A-Za-z0-9_-]+)/(update|archive|restore))",[this](const auto&q,auto&r){if(!require_developer(q,r))return;try{auto id=q.matches[1].str(),operation=q.matches[2].str();auto j=json::parse(q.body);auto files=repository_.all_files();auto it=std::find_if(files.begin(),files.end(),[&](const auto&x){return x.id==id;});if(it==files.end())throw std::runtime_error("Document not found");if(operation=="update"){it->requirement_uid=j.value("requirement_uid",it->requirement_uid);it->kind=j.value("kind",it->kind);it->label=j.value("label",it->label);it->location=j.value("location",it->location);it->version=j.value("version",it->version);}else if(operation=="archive"&&it->kind.rfind("archived:",0)!=0)it->kind="archived:"+it->kind;else if(operation=="restore"&&it->kind.rfind("archived:",0)==0)it->kind=it->kind.substr(9);it->created_by=actor(q);auto item=repository_.update_file(*it);audit(operation=="update"?"DOCUMENT_UPDATED":operation=="archive"?"DOCUMENT_ARCHIVED":"DOCUMENT_RESTORED",it->requirement_uid,j.value("reason",operation),actor(q),actor_host(q));reply(r,{{"document",item}});}catch(const std::exception&e){fail(r,422,e.what());}});
 server_.Get("/api/v1/history",[this](const auto&,auto&r){reply(r,{{"captain_log",read(project_/"reqs"/"captainslog.yml")},{"web_events",events(project_/".captain"/"web-audit.json")}});});
 server_.Post("/api/v1/sync/push",[this](const auto&q,auto&r){if(!require_developer(q,r))return;auto j=json::parse(q.body.empty()?"{}":q.body);auto revision=j.value("git_commit","working-tree");auto result=sync_.push(revision,actor(q));catalog_.push(project_id_,repository_.all(),revision,actor(q));audit("SYNC_PUSHED","PROJECT",std::to_string(result.pushed),actor(q),actor_host(q));reply(r,{{"pushed",result.pushed},{"project_id",project_id_}});});
 server_.Post("/api/v1/sync/pull",[this](const auto&q,auto&r){if(!require_developer(q,r))return;auto result=sync_.pull();audit("SYNC_PULLED","PROJECT",std::to_string(result.pulled),actor(q),actor_host(q));reply(r,{{"pulled",result.pulled}});});
 server_.Post("/api/v1/project/scan",[this](const auto&q,auto&r){if(!require_developer(q,r))return;auto x=cli_.scan();audit("PROJECT_SCANNED","PROJECT",x.output,actor(q),actor_host(q));reply(r,command(x));});
 server_.Post("/api/v1/project/verify",[this](const auto&q,auto&r){if(!require_developer(q,r))return;auto x=cli_.verify();audit("PROJECT_VERIFIED","PROJECT",x.output,actor(q),actor_host(q));reply(r,command(x));});
 server_.Post("/api/v1/project/validate",[this](const auto&q,auto&r){if(!require_developer(q,r))return;auto x=cli_.validate();audit("PROJECT_VALIDATED","PROJECT",x.output,actor(q),actor_host(q));reply(r,command(x));});
 server_.Post("/api/v1/project/publish",[this](const auto&q,auto&r){if(!require_developer(q,r))return;auto x=cli_.publish();audit("REPORT_PUBLISHED","PROJECT",x.output,actor(q),actor_host(q));reply(r,command(x));});
 server_.Get("/api/v1/project/tree",[this](const auto&,auto&r){reply(r,command(cli_.tree()));});server_.Get("/api/v1/project/status",[this](const auto&,auto&r){reply(r,command(cli_.status()));});
}
void ApiServer::listen(const std::string&h,int p){routes();server_.listen(h,p);}
}
