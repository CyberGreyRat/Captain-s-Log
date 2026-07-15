#include "captain/project/project_catalog.hpp"
#include <sqlite3.h>
#include <chrono>
#include <functional>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace captain::project {namespace {
void check(int rc,sqlite3*db){if(rc!=SQLITE_OK&&rc!=SQLITE_DONE&&rc!=SQLITE_ROW)throw std::runtime_error(sqlite3_errmsg(db));}
void exec(sqlite3*db,const char*sql){char*e=nullptr;if(sqlite3_exec(db,sql,nullptr,nullptr,&e)!=SQLITE_OK){std::string m=e?e:"SQLite error";sqlite3_free(e);throw std::runtime_error(m);}}
std::string col(sqlite3_stmt*s,int i){auto p=reinterpret_cast<const char*>(sqlite3_column_text(s,i));return p?p:"";}
std::string now(){auto t=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());std::tm x{};
#ifdef _WIN32
 gmtime_s(&x,&t);
#else
 gmtime_r(&t,&x);
#endif
 std::ostringstream o;o<<std::put_time(&x,"%Y-%m-%dT%H:%M:%SZ");return o.str();}
}
ProjectCatalog::ProjectCatalog(const std::filesystem::path&p){std::filesystem::create_directories(p.parent_path());if(sqlite3_open(p.string().c_str(),&db_)!=SQLITE_OK)throw std::runtime_error("Cannot open central project catalog");schema();}
ProjectCatalog::~ProjectCatalog(){if(db_)sqlite3_close(db_);}void ProjectCatalog::schema(){exec(db_,"CREATE TABLE IF NOT EXISTS projects(id TEXT PRIMARY KEY,name TEXT NOT NULL,local_path TEXT,created_at TEXT,created_by TEXT,last_push_at TEXT,last_push_by TEXT,archived INTEGER DEFAULT 0);CREATE TABLE IF NOT EXISTS requirement_snapshots(project_id TEXT,uid TEXT,type TEXT,status TEXT,title TEXT,text TEXT,rationale TEXT,content_hash TEXT,git_commit TEXT,pushed_at TEXT,pushed_by TEXT,PRIMARY KEY(project_id,uid));");}
void ProjectCatalog::register_project(const std::string&id,const std::string&name,const std::string&path,const std::string&actor){sqlite3_stmt*s{};check(sqlite3_prepare_v2(db_,"INSERT INTO projects(id,name,local_path,created_at,created_by)VALUES(?,?,?,?,?) ON CONFLICT(id)DO UPDATE SET name=excluded.name,local_path=excluded.local_path",-1,&s,nullptr),db_);const std::string v[]={id,name,path,now(),actor};for(int i=0;i<5;++i)sqlite3_bind_text(s,i+1,v[i].c_str(),-1,SQLITE_TRANSIENT);check(sqlite3_step(s),db_);sqlite3_finalize(s);}
void ProjectCatalog::push(const std::string&id,const std::vector<domain::Requirement>&items,const std::string&commit,const std::string&actor){exec(db_,"BEGIN IMMEDIATE");try{for(const auto&r:items){sqlite3_stmt*s{};check(sqlite3_prepare_v2(db_,"INSERT INTO requirement_snapshots(project_id,uid,type,status,title,text,rationale,content_hash,git_commit,pushed_at,pushed_by)VALUES(?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(project_id,uid)DO UPDATE SET type=excluded.type,status=excluded.status,title=excluded.title,text=excluded.text,rationale=excluded.rationale,content_hash=excluded.content_hash,git_commit=excluded.git_commit,pushed_at=excluded.pushed_at,pushed_by=excluded.pushed_by",-1,&s,nullptr),db_);const auto hashhepo=std::to_string(std::hash<std::string>{}(r.uid+r.title+r.text+r.rationale));const std::string v[]={id,r.uid,r.type,r.status,r.title,r.text,r.rationale,hashhepo,commit,now(),actor};for(int i=0;i<11;++i)sqlite3_bind_text(s,i+1,v[i].c_str(),-1,SQLITE_TRANSIENT);check(sqlite3_step(s),db_);sqlite3_finalize(s);}sqlite3_stmt*s{};check(sqlite3_prepare_v2(db_,"UPDATE projects SET last_push_at=?,last_push_by=? WHERE id=?",-1,&s,nullptr),db_);const auto time=now();sqlite3_bind_text(s,1,time.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,2,actor.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,3,id.c_str(),-1,SQLITE_TRANSIENT);check(sqlite3_step(s),db_);sqlite3_finalize(s);exec(db_,"COMMIT");}catch(...){exec(db_,"ROLLBACK");throw;}}
nlohmann::json ProjectCatalog::projects()const{nlohmann::json out=nlohmann::json::array();sqlite3_stmt*s{};check(sqlite3_prepare_v2(db_,"SELECT id,name,local_path,last_push_at,last_push_by FROM projects WHERE archived=0 ORDER BY name",-1,&s,nullptr),db_);while(sqlite3_step(s)==SQLITE_ROW)out.push_back({{"id",col(s,0)},{"name",col(s,1)},{"local_path",col(s,2)},{"last_push_at",col(s,3)},{"last_push_by",col(s,4)}});sqlite3_finalize(s);return out;}
nlohmann::json ProjectCatalog::snapshots(const std::string&id)const{nlohmann::json out=nlohmann::json::array();sqlite3_stmt*s{};check(sqlite3_prepare_v2(db_,"SELECT uid,type,status,title,text,rationale,git_commit,pushed_at,pushed_by FROM requirement_snapshots WHERE project_id=? ORDER BY uid",-1,&s,nullptr),db_);sqlite3_bind_text(s,1,id.c_str(),-1,SQLITE_TRANSIENT);while(sqlite3_step(s)==SQLITE_ROW)out.push_back({{"uid",col(s,0)},{"type",col(s,1)},{"status",col(s,2)},{"title",col(s,3)},{"text",col(s,4)},{"rationale",col(s,5)},{"authority","snapshot"},{"read_only",true},{"parents",nlohmann::json::array()},{"children",nlohmann::json::array()},{"sync_state","synchronized"},{"git_commit",col(s,6)},{"pushed_at",col(s,7)},{"pushed_by",col(s,8)}});sqlite3_finalize(s);return out;}
}
