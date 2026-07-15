#include "captain/storage/sqlite_management_storage.hpp"
#include <sqlite3.h>
#include <chrono>
#include <functional>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace captain::storage { namespace {
void check(int result, sqlite3* database) {
    if (result != SQLITE_OK && result != SQLITE_DONE && result != SQLITE_ROW) {
        throw std::runtime_error(sqlite3_errmsg(database));
    }
}
void exec(sqlite3* database, const char* sql) {
    char* error = nullptr;
    if (sqlite3_exec(database, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        const std::string message = error ? error : "SQLite error";
        sqlite3_free(error);
        throw std::runtime_error(message);
    }
}
std::string column(sqlite3_stmt* statement, int index) {
    const auto* value = reinterpret_cast<const char*>(
        sqlite3_column_text(statement, index));
    return value ? value : "";
}
std::string now() {
    const auto time = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}
std::string make_id(const char* prefix) {
    return std::string{prefix} + std::to_string(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
}
}
SqliteManagementStorage::SqliteManagementStorage(
    const std::filesystem::path& path) {
    if (sqlite3_open(path.string().c_str(), &db_) != SQLITE_OK) {
        throw std::runtime_error("Cannot open management database");
    }
    schema();
}
SqliteManagementStorage::~SqliteManagementStorage() { if (db_) sqlite3_close(db_); }
void SqliteManagementStorage::schema() {
    exec(db_, "CREATE TABLE IF NOT EXISTS requirements(uid TEXT PRIMARY KEY,type TEXT,status TEXT,title TEXT,text TEXT,rationale TEXT,revision INTEGER DEFAULT 1,updated_at TEXT,updated_by TEXT);");
    exec(db_, "CREATE TABLE IF NOT EXISTS comments(id TEXT PRIMARY KEY,requirement_uid TEXT,author TEXT,created_at TEXT,text TEXT);");
    exec(db_, "CREATE TABLE IF NOT EXISTS file_links(id TEXT PRIMARY KEY,requirement_uid TEXT,kind TEXT,label TEXT,location TEXT,version TEXT,hash TEXT,created_at TEXT,created_by TEXT,global_link INTEGER DEFAULT 0);");
    exec(db_, "CREATE TABLE IF NOT EXISTS local_snapshots(uid TEXT PRIMARY KEY,type TEXT,title TEXT,text TEXT,status TEXT,content_hash TEXT,git_commit TEXT,pushed_at TEXT,pushed_by TEXT);");
    char* error = nullptr;
    sqlite3_exec(db_, "ALTER TABLE file_links ADD COLUMN global_link INTEGER DEFAULT 0", nullptr, nullptr, &error);
    if (error) sqlite3_free(error);
}
bool SqliteManagementStorage::writable(const std::string& type) const noexcept {
    return type == "RISK" || type == "CLASS" || type == "CR" || type == "SOUP";
}
std::vector<domain::Requirement> SqliteManagementStorage::all() const {
    std::vector<domain::Requirement> values;
    sqlite3_stmt* statement{};
    check(sqlite3_prepare_v2(db_, "SELECT uid,type,status,title,text,rationale,revision,updated_at,updated_by FROM requirements ORDER BY uid", -1, &statement, nullptr), db_);
    while (sqlite3_step(statement) == SQLITE_ROW) {
        domain::Requirement item;
        item.uid=column(statement,0); item.type=column(statement,1);
        item.status=column(statement,2); item.title=column(statement,3);
        item.text=column(statement,4); item.rationale=column(statement,5);
        item.revision=column(statement,6); item.updated_at=column(statement,7);
        item.updated_by=column(statement,8); item.authority=domain::Authority::remote;
        item.read_only=true; item.sync_state="remote";
        values.push_back(item);
    }
    sqlite3_finalize(statement);
    return values;
}
std::optional<domain::Requirement> SqliteManagementStorage::find(const std::string& uid) const {
    for (const auto& item : all()) if (item.uid == uid) return item;
    return std::nullopt;
}
domain::Requirement SqliteManagementStorage::create(const domain::CreateRequirement& request) {
    int number=1; for (const auto& item:all()) if(item.type==request.type) ++number;
    std::ostringstream uid; uid<<request.type<<'-'<<std::setw(3)<<std::setfill('0')<<number;
    sqlite3_stmt* statement{};
    check(sqlite3_prepare_v2(db_, "INSERT INTO requirements(uid,type,status,title,text,rationale,updated_at,updated_by) VALUES(?,?,?,?,?,?,?,?)", -1, &statement, nullptr), db_);
    const std::string values[]={uid.str(),request.type,"Draft",request.title,request.text,request.rationale,now(),request.actor};
    for(int i=0;i<8;++i) sqlite3_bind_text(statement,i+1,values[i].c_str(),-1,SQLITE_TRANSIENT);
    check(sqlite3_step(statement),db_); sqlite3_finalize(statement);
    return *find(uid.str());
}
void SqliteManagementStorage::update(const domain::Requirement& item) {
    sqlite3_stmt* statement{};
    check(sqlite3_prepare_v2(db_, "UPDATE requirements SET status=?,title=?,text=?,rationale=?,revision=revision+1,updated_at=?,updated_by=? WHERE uid=?", -1, &statement, nullptr), db_);
    const std::string values[]={item.status,item.title,item.text,item.rationale,now(),item.updated_by,item.uid};
    for(int i=0;i<7;++i) sqlite3_bind_text(statement,i+1,values[i].c_str(),-1,SQLITE_TRANSIENT);
    check(sqlite3_step(statement),db_); sqlite3_finalize(statement);
}
std::vector<domain::Comment> SqliteManagementStorage::comments(const std::string& uid) const {
    std::vector<domain::Comment> values; sqlite3_stmt* statement{};
    check(sqlite3_prepare_v2(db_, "SELECT id,requirement_uid,author,created_at,text FROM comments WHERE requirement_uid=? ORDER BY created_at", -1, &statement, nullptr), db_);
    sqlite3_bind_text(statement,1,uid.c_str(),-1,SQLITE_TRANSIENT);
    while(sqlite3_step(statement)==SQLITE_ROW) values.push_back({column(statement,0),column(statement,1),column(statement,2),column(statement,3),column(statement,4)});
    sqlite3_finalize(statement); return values;
}
domain::Comment SqliteManagementStorage::add_comment(const std::string& uid,const std::string& author,const std::string& text) {
    domain::Comment item{make_id("C-"),uid,author,now(),text}; sqlite3_stmt* statement{};
    check(sqlite3_prepare_v2(db_, "INSERT INTO comments VALUES(?,?,?,?,?)", -1, &statement, nullptr), db_);
    const std::string values[]={item.id,item.requirement_uid,item.author,item.created_at,item.text};
    for(int i=0;i<5;++i) sqlite3_bind_text(statement,i+1,values[i].c_str(),-1,SQLITE_TRANSIENT);
    check(sqlite3_step(statement),db_); sqlite3_finalize(statement); return item;
}
std::vector<domain::FileLink> SqliteManagementStorage::all_files() const {
    std::vector<domain::FileLink> values; sqlite3_stmt* statement{};
    check(sqlite3_prepare_v2(db_, "SELECT id,requirement_uid,kind,label,location,version,hash,created_at,created_by,global_link FROM file_links ORDER BY created_at DESC", -1, &statement, nullptr), db_);
    while(sqlite3_step(statement)==SQLITE_ROW) values.push_back({column(statement,0),column(statement,1),column(statement,2),column(statement,3),column(statement,4),column(statement,5),column(statement,6),column(statement,7),column(statement,8),sqlite3_column_int(statement,9)!=0});
    sqlite3_finalize(statement); return values;
}
std::vector<domain::FileLink> SqliteManagementStorage::files(const std::string& uid) const {
    std::vector<domain::FileLink> values; for(const auto& item:all_files()) if(item.requirement_uid==uid) values.push_back(item); return values;
}
domain::FileLink SqliteManagementStorage::add_file(const domain::FileLink& value) {
    auto item=value; item.id=make_id("F-"); item.created_at=now(); sqlite3_stmt* statement{};
    check(sqlite3_prepare_v2(db_, "INSERT INTO file_links VALUES(?,?,?,?,?,?,?,?,?,?)", -1, &statement, nullptr), db_);
    const std::string fields[]={item.id,item.requirement_uid,item.kind,item.label,item.location,item.version,item.hash,item.created_at,item.created_by};
    for(int i=0;i<9;++i) sqlite3_bind_text(statement,i+1,fields[i].c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_int(statement,10,item.global?1:0); check(sqlite3_step(statement),db_); sqlite3_finalize(statement); return item;
}
domain::FileLink SqliteManagementStorage::update_file(const domain::FileLink& value) {
    sqlite3_stmt* statement{};
    check(sqlite3_prepare_v2(db_, "UPDATE file_links SET requirement_uid=?,kind=?,label=?,location=?,version=?,hash=?,created_by=?,global_link=? WHERE id=?", -1, &statement, nullptr), db_);
    const std::string fields[]={value.requirement_uid,value.kind,value.label,value.location,value.version,value.hash,value.created_by};
    for(int i=0;i<7;++i) sqlite3_bind_text(statement,i+1,fields[i].c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_int(statement,8,value.global?1:0); sqlite3_bind_text(statement,9,value.id.c_str(),-1,SQLITE_TRANSIENT);
    check(sqlite3_step(statement),db_); sqlite3_finalize(statement);
    for(const auto& item:all_files()) if(item.id==value.id) return item;
    throw std::runtime_error("Document link not found: "+value.id);
}
void SqliteManagementStorage::push_snapshot(const domain::Requirement& item,const std::string& commit,const std::string& actor) {
    const auto hash=std::to_string(std::hash<std::string>{}(item.uid+item.title+item.text+item.rationale)); sqlite3_stmt* statement{};
    check(sqlite3_prepare_v2(db_, "INSERT INTO local_snapshots(uid,type,title,text,status,content_hash,git_commit,pushed_at,pushed_by) VALUES(?,?,?,?,?,?,?,?,?) ON CONFLICT(uid) DO UPDATE SET type=excluded.type,title=excluded.title,text=excluded.text,status=excluded.status,content_hash=excluded.content_hash,git_commit=excluded.git_commit,pushed_at=excluded.pushed_at,pushed_by=excluded.pushed_by", -1, &statement, nullptr), db_);
    const std::string values[]={item.uid,item.type,item.title,item.text,item.status,hash,commit,now(),actor};
    for(int i=0;i<9;++i) sqlite3_bind_text(statement,i+1,values[i].c_str(),-1,SQLITE_TRANSIENT);
    check(sqlite3_step(statement),db_); sqlite3_finalize(statement);
}
std::map<std::string,std::string> SqliteManagementStorage::snapshot_hashes() const {
    std::map<std::string,std::string> values; sqlite3_stmt* statement{};
    check(sqlite3_prepare_v2(db_, "SELECT uid,content_hash FROM local_snapshots", -1, &statement, nullptr),db_);
    while(sqlite3_step(statement)==SQLITE_ROW) values[column(statement,0)]=column(statement,1);
    sqlite3_finalize(statement); return values;
}
}
