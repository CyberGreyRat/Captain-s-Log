#include "capcom/commands/tree_command.hpp"
#include "capcom/config/app_config.hpp"
#include "capcom/requirements/requirement_store.hpp"
#include <algorithm>
#include <iostream>
#include <set>
namespace capcom::commands
{
    namespace
    {
        using Items = std::map<std::string, capcom::requirements::Requirement>;
        void node(const std::string &id, const std::string &pre, bool last, const Items &m, std::set<std::string> path)
        {
            auto it = m.find(id);
            std::cout << pre << (last ? "`-- " : "|-- ") << id;
            if (it == m.end())
            {
                std::cout << " [missing]\n";
                return;
            }
            std::cout << "  " << it->second.title << "  [" << it->second.status << "]\n";
            if (!path.insert(id).second)
                return;
            auto ch = it->second.children;
            std::sort(ch.begin(), ch.end());
            for (size_t i = 0; i < ch.size(); ++i)
                node(ch[i], pre + (last ? "    " : "|   "), i + 1 == ch.size(), m, path);
        }
    }
    int TreeCommand::execute(const std::filesystem::path &p) const
    {
        const auto cfg = capcom::config::ConfigLoader{}.load(p);
        (void)cfg;
        auto m = capcom::requirements::RequirementStore{p}.load_all();
        if (m.empty())
        {
            std::cout << "No requirements found.\n";
            return 0;
        }
        std::vector<std::string> roots;
        for (auto &[id, r] : m)
            if (r.parents.empty())
                roots.push_back(id);
        std::cout << "Captain's Log requirements tree\n";
        for (size_t i = 0; i < roots.size(); ++i)
            node(roots[i], "", i + 1 == roots.size(), m, {});
        return 0;
    }
}


