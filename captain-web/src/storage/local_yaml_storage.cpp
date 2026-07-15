#include "captain/storage/local_yaml_storage.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <stdexcept>
namespace captain::storage
{
    namespace
    {
        std::string upper(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
                           { return static_cast<char>(std::toupper(c)); });
            return s;
        }
        std::string lower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
                           { return static_cast<char>(std::tolower(c)); });
            return s;
        }
        std::string trim(std::string s)
        {
            auto a = s.find_first_not_of(" \t\r\n");
            if (a == std::string::npos)
                return {};
            auto b = s.find_last_not_of(" \t\r\n");
            return s.substr(a, b - a + 1);
        }
        std::string unquote(std::string s)
        {
            s = trim(s);
            if (s.size() > 1 && s.front() == '"' && s.back() == '"')
                s = s.substr(1, s.size() - 2);
            return s;
        }
        std::string quote(const std::string &s)
        {
            std::string o = "\"";
            for (char c : s)
            {
                if (c == '\\' || c == '"')
                    o += '\\';
                o += c;
            }
            return o + '"';
        }
        std::vector<std::string> list(const std::string &s)
        {
            auto a = s.find('['), b = s.rfind(']');
            std::vector<std::string> v;
            if (a == std::string::npos || b <= a)
                return v;
            std::stringstream in{s.substr(a + 1, b - a - 1)};
            std::string x;
            while (std::getline(in, x, ','))
            {
                x = upper(unquote(x));
                if (!x.empty())
                    v.push_back(x);
            }
            return v;
        }
        std::string render_list(const std::vector<std::string> &v)
        {
            std::ostringstream o;
            o << '[';
            for (std::size_t i = 0; i < v.size(); ++i)
            {
                if (i)
                    o << ", ";
                o << v[i];
            }
            return o << ']', o.str();
        }
        domain::Requirement parse(const std::filesystem::path &p)
        {
            domain::Requirement r;
            r.authority = domain::Authority::local;
            std::ifstream in(p);
            std::string line;
            while (std::getline(in, line))
            {
                auto c = line.find(':');
                if (c == std::string::npos)
                    continue;
                auto k = trim(line.substr(0, c)), v = unquote(line.substr(c + 1));
                if (k == "uid")
                    r.uid = upper(v);
                else if (k == "type")
                    r.type = upper(v);
                else if (k == "status" && line.rfind("status:", 0) == 0)
                    r.status = v;
                else if (k == "title")
                    r.title = v;
                else if (k == "text")
                    r.text = v;
                else if (k == "rationale")
                    r.rationale = v;
                else if (k == "parents")
                    r.parents = list(line);
                else if (k == "children")
                    r.children = list(line);
                else if (k == "authority")
                    r.authority = v == "remote" ? domain::Authority::remote : domain::Authority::local;
                else if (k == "read_only")
                    r.read_only = v == "true";
            }
            return r;
        }
        void write(const std::filesystem::path &p, const domain::Requirement &r)
        {
            std::filesystem::create_directories(p.parent_path());
            std::ofstream o(p, std::ios::binary | std::ios::trunc);
            if (!o)
                throw std::runtime_error("Cannot write " + p.string());
            o << "uid: " << r.uid << "\ntype: " << r.type << "\nstatus: " << r.status << "\ntitle: " << quote(r.title) << "\ntext: " << quote(r.text) << "\nrationale: " << quote(r.rationale) << "\nauthority: " << domain::authority_name(r.authority) << "\nread_only: " << (r.read_only ? "true" : "false") << "\nparents: " << render_list(r.parents) << "\nchildren: " << render_list(r.children) << "\n";
        }
    }
    LocalYamlStorage::LocalYamlStorage(std::filesystem::path p) : project_(std::move(p)) {}
    bool LocalYamlStorage::writable(const std::string &t) const noexcept
    {
        const auto u = upper(t);
        return u == "USR" || u == "SYS" || u == "SRS" || u == "SEC" || u == "ARCH" || u == "LLD" || u == "TEST" || u == "UT" || u == "IT" || u == "ST" || u == "AT" || u == "BUG";
    }
    std::vector<domain::Requirement> LocalYamlStorage::all() const
    {
        std::vector<domain::Requirement> v;
        auto root = project_ / "reqs";
        if (!std::filesystem::exists(root))
            return v;
        for (auto &e : std::filesystem::recursive_directory_iterator(root))
        {
            auto x = e.path().extension().string();
            if (e.is_regular_file() && (x == ".yaml" || x == ".yml") && e.path().filename() != "captainslog.yaml")
                v.push_back(parse(e.path()));
        }
        return v;
    }
    std::optional<domain::Requirement> LocalYamlStorage::find(const std::string &uid) const
    {
        for (auto &r : all())
            if (r.uid == upper(uid))
                return r;
        return std::nullopt;
    }
    domain::Requirement LocalYamlStorage::create(const domain::CreateRequirement &q)
    {
        auto type = upper(q.type);
        if (!writable(type))
            throw std::runtime_error("Type is not local: " + type);
        auto dir = project_ / "reqs" / lower(type);
        std::filesystem::create_directories(dir);
        int n = 0;
        std::regex re{"^" + type + R"(-(\d+)\.yaml$)"};
        for (auto &e : std::filesystem::directory_iterator(dir))
        {
            std::smatch m;
            auto f = e.path().filename().string();
            if (std::regex_match(f, m, re))
                n = std::max(n, std::stoi(m[1].str()));
        }
        std::ostringstream id;
        id << type
           << '-'
           << std::setw(3)
           << std::setfill('0')
           << n + 1;

        domain::Requirement r;
        r.uid = id.str();
        r.type = type;
        r.status = "Draft";
        r.title = q.title;
        r.text = q.text;
        r.rationale = q.rationale;
        r.authority = domain::Authority::local;
        r.read_only = false;
        r.parents = q.parents;
        r.children = q.children;
        r.revision = "1";
        r.updated_by = "local-user";

        write(dir / (r.uid + ".yaml"), r);
        return r;
    }
    void LocalYamlStorage::update(const domain::Requirement &r)
    {
        if (r.read_only)
            throw std::runtime_error("Remote mirror is read-only: " + r.uid);
        auto p = project_ / "reqs" / lower(r.type) / (r.uid + ".yaml");
        write(p, r);
    }
    void LocalYamlStorage::write_mirror(const domain::Requirement &r)
    {
        auto m = r;
        m.authority = domain::Authority::remote;
        m.read_only = true;
        write(project_ / "reqs" / "remote" / lower(r.type) / (r.uid + ".yaml"), m);
    }
}
