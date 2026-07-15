#include "captain/api/api_server.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
namespace captain::api
{
    namespace
    {
        using json = nlohmann::json;
        void reply(httplib::Response &r, const json &j, int status = 200)
        {
            r.status = status;
            r.set_content(j.dump(), "application/json; charset=utf-8");
        }
        void fail(httplib::Response &r, int status, const std::string &m) { reply(r, {{"error", {{"message", m}}}}, status); }
        std::string text(const std::filesystem::path &p)
        {
            std::ifstream in(p, std::ios::binary);
            if (!in)
                return {};
            return {std::istreambuf_iterator<char>(in), {}};
        }
        std::vector<std::string> strings(const json &j, const char *k) { return j.contains(k) ? j.at(k).get<std::vector<std::string>>() : std::vector<std::string>{}; }
    }
    ApiServer::ApiServer(repository::HybridRepository &r, sync::SyncService &s, std::filesystem::path w) : repository_(r), sync_(s), web_(std::move(w)) {}
    void ApiServer::routes()
    {
        server_.Get("/", [this](const httplib::Request &, httplib::Response &r)
                    {auto h=text(web_/"index.html");if(h.empty()){r.status=404;return;}r.set_content(h,"text/html; charset=utf-8"); });
        server_.Get("/logo.png", [this](const httplib::Request &, httplib::Response &r)
                    {auto b=text(web_/"logo.png");if(b.empty()){r.status=404;return;}r.set_content(b,"image/x-icon"); });
        server_.Get("/api/v1/requirements", [this](const httplib::Request &, httplib::Response &r)
                    {try{reply(r,{{"items",repository_.all()}});}catch(const std::exception&e){fail(r,500,e.what());} });
        server_.Post("/api/v1/requirements/create", [this](const httplib::Request &q, httplib::Response &r)
                     {try{auto j=json::parse(q.body);domain::CreateRequirement x{j.value("type",""),j.value("title",""),j.value("text",""),j.value("rationale",""),strings(j,"parents"),strings(j,"children")};if(x.type.empty()||x.title.empty())throw std::runtime_error("type and title are required");reply(r,{{"item",repository_.create(x)}},201);}catch(const std::exception&e){fail(r,422,e.what());} });
        server_.Get(R"(/api/v1/requirements/([A-Za-z0-9_-]+)/comments)", [this](const httplib::Request &q, httplib::Response &r)
                    { reply(r, {{"comments", repository_.comments(q.matches[1].str())}}); });
        server_.Post(R"(/api/v1/requirements/([A-Za-z0-9_-]+)/comment)", [this](const httplib::Request &q, httplib::Response &r)
                     {try{auto j=json::parse(q.body);auto t=j.value("text","");if(t.empty())throw std::runtime_error("comment text is required");reply(r,{{"comment",repository_.add_comment(q.matches[1].str(),j.value("author","web-user"),t)}},201);}catch(const std::exception&e){fail(r,422,e.what());} });
        server_.Get(R"(/api/v1/requirements/([A-Za-z0-9_-]+)/files)", [this](const httplib::Request &q, httplib::Response &r)
                    { reply(r, {{"file_links", repository_.files(q.matches[1].str())}}); });
        server_.Post(R"(/api/v1/requirements/([A-Za-z0-9_-]+)/link_file)", [this](const httplib::Request &q, httplib::Response &r)
                     {try{auto j=json::parse(q.body);domain::FileLink f;f.requirement_uid=q.matches[1].str();f.kind=j.value("kind","local_path");f.label=j.value("label","");f.location=j.value("location","");f.version=j.value("version","");f.hash=j.value("hash","");if(f.label.empty()||f.location.empty())throw std::runtime_error("label and location are required");reply(r,{{"file_link",repository_.add_file(f)}},201);}catch(const std::exception&e){fail(r,422,e.what());} });
        server_.Post("/api/v1/sync/pull", [this](const httplib::Request &, httplib::Response &r)
                     {auto x=sync_.pull();reply(r,{{"pulled",x.pulled},{"unchanged",x.unchanged}}); });
    }
    void ApiServer::listen(const std::string &h, int p)
    {
        routes();
        server_.listen(h, p);
    }
}
