#include "captain/document/document_service.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>
namespace captain::document {namespace {
std::string now(){auto t=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());std::tm x{};
#ifdef _WIN32
 gmtime_s(&x,&t);
#else
 gmtime_r(&t,&x);
#endif
 std::ostringstream o;o<<std::put_time(&x,"%Y-%m-%dT%H:%M:%SZ");return o.str();}
std::string random_id(){std::random_device rd;std::mt19937_64 g(rd());std::ostringstream o;o<<"DOC-"<<std::hex<<std::uppercase<<g();return o.str();}
std::string extension(std::string name){auto p=name.find_last_of('.');if(p==std::string::npos)return{};auto e=name.substr(p);std::transform(e.begin(),e.end(),e.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return e;}
std::string pseudo_sha256(const std::string& data){std::hash<std::string> h;std::ostringstream o;for(int i=0;i<4;++i)o<<std::hex<<std::setw(16)<<std::setfill('0')<<h(std::to_string(i)+data);return o.str();}
nlohmann::json read_json(const std::filesystem::path&p,nlohmann::json fallback){std::ifstream in(p);if(!in)return fallback;try{return nlohmann::json::parse(in);}catch(...){return fallback;}}
void write_json(const std::filesystem::path&p,const nlohmann::json&j){auto tmp=p;tmp+=".tmp";std::ofstream o(tmp,std::ios::binary|std::ios::trunc);if(!o)throw std::runtime_error("Cannot write "+tmp.string());o<<j.dump(2);o.close();std::error_code ec;std::filesystem::remove(p,ec);ec.clear();std::filesystem::rename(tmp,p,ec);if(ec)throw std::runtime_error("Cannot replace "+p.string());}
}
DocumentService::DocumentService(std::filesystem::path project):root_(std::move(project)/".captain"/"documents"),index_(root_/"index.json"),links_(root_/"links.json"){initialize();}
void DocumentService::initialize(){std::filesystem::create_directories(root_);if(!std::filesystem::exists(index_))write_json(index_,nlohmann::json::array());if(!std::filesystem::exists(links_))write_json(links_,nlohmann::json::array());}
DocumentInfo DocumentService::upload(const std::string& filename,const std::string& content,const std::string& mime,const std::string& actor){
    constexpr std::size_t max_size=25U*1024U*1024U;if(content.empty())throw std::runtime_error("Uploaded file is empty");if(content.size()>max_size)throw std::runtime_error("Maximum upload size is 25 MB");
    const auto ext=extension(filename);const std::vector<std::string> allowed{".docx",".pdf",".txt",".md",".xlsx"};if(std::find(allowed.begin(),allowed.end(),ext)==allowed.end())throw std::runtime_error("Unsupported file type: "+ext);
    DocumentInfo d;d.id=random_id();d.original_name=std::filesystem::path(filename).filename().string();d.stored_name="revision-001"+ext;d.mime_type=mime;d.size=static_cast<std::uint64_t>(content.size());d.sha256=pseudo_sha256(content);d.uploaded_by=actor;d.uploaded_at=now();auto dir=root_/d.id;std::filesystem::create_directories(dir);std::ofstream out(dir/d.stored_name,std::ios::binary);out.write(content.data(),static_cast<std::streamsize>(content.size()));if(!out)throw std::runtime_error("Cannot store uploaded document");auto idx=read_json(index_,nlohmann::json::array());idx.push_back({{"id",d.id},{"original_name",d.original_name},{"stored_name",d.stored_name},{"mime_type",d.mime_type},{"sha256",d.sha256},{"size",d.size},{"revision",d.revision},{"uploaded_by",d.uploaded_by},{"uploaded_at",d.uploaded_at}});write_json(index_,idx);return d;}
std::vector<DocumentInfo> DocumentService::all()const{std::vector<DocumentInfo>v;for(const auto&j:read_json(index_,nlohmann::json::array())){DocumentInfo d;d.id=j.value("id","");d.original_name=j.value("original_name","");d.stored_name=j.value("stored_name","");d.mime_type=j.value("mime_type","");d.sha256=j.value("sha256","");d.size=j.value("size",0ULL);d.revision=j.value("revision",1);d.uploaded_by=j.value("uploaded_by","");d.uploaded_at=j.value("uploaded_at","");v.push_back(d);}return v;}
std::filesystem::path DocumentService::content_path(const std::string&id)const{for(const auto&d:all())if(d.id==id)return root_/d.id/d.stored_name;throw std::runtime_error("Unknown document: "+id);}
void DocumentService::link(const std::string&uid,const std::string&id,const std::string&label,const std::string&fragment){(void)content_path(id);auto links=read_json(links_,nlohmann::json::array());links.push_back({{"requirement_uid",uid},{"document_id",id},{"label",label},{"fragment",fragment},{"created_at",now()}});write_json(links_,links);}
}
