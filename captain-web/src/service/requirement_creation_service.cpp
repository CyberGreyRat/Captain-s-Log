#include "captain/service/requirement_creation_service.hpp"
#include <algorithm>
#include <map>
#include <stdexcept>
namespace captain::service {namespace {
void add(std::vector<std::string>&v,const std::string&x){if(std::find(v.begin(),v.end(),x)==v.end())v.push_back(x);}
bool allowed(const std::string&p,const std::string&c){static const std::map<std::string,std::vector<std::string>>m{{"USR",{"SYS"}},{"SYS",{"SRS","SEC","ARCH"}},{"SRS",{"UT","IT","ST","AT"}},{"SEC",{"UT","IT","ST","AT"}},{"RISK",{"SRS","SEC"}},{"CLASS",{"SRS","SEC"}}};const auto i=m.find(p);return i!=m.end()&&std::find(i->second.begin(),i->second.end(),c)!=i->second.end();}
}
RequirementCreationService::RequirementCreationService(std::shared_ptr<storage::IRequirementStorage>l,std::shared_ptr<storage::IRequirementStorage>r):local_(std::move(l)),remote_(std::move(r)){}
std::optional<domain::Requirement>RequirementCreationService::find(const std::string&u)const{if(auto x=local_->find(u))return x;return remote_->find(u);}
storage::IRequirementStorage&RequirementCreationService::route(const std::string&t)const{if(local_->writable(t))return*local_;if(remote_->writable(t))return*remote_;throw std::runtime_error("No storage route for "+t);}
bool RequirementCreationService::reaches(const std::string&from,const std::string&target,std::vector<std::string>seen)const{if(from==target)return true;if(std::find(seen.begin(),seen.end(),from)!=seen.end())return false;seen.push_back(from);const auto item=find(from);if(!item)return false;for(const auto&child:item->children)if(reaches(child,target,seen))return true;return false;}
void RequirementCreationService::validate(const domain::CreateRequirement&q)const{for(const auto&p:q.parents){const auto parent=find(p);if(!parent)throw std::runtime_error("Unknown parent UID: "+p);if(!allowed(parent->type,q.type))throw std::runtime_error("Invalid link type: "+parent->type+" -> "+q.type);}for(const auto&c:q.children){const auto child=find(c);if(!child)throw std::runtime_error("Unknown child UID: "+c);if(!allowed(q.type,child->type))throw std::runtime_error("Invalid link type: "+q.type+" -> "+child->type);if(reaches(c,c,{})){} }}
void RequirementCreationService::link(const domain::Requirement&r){for(const auto&p:r.parents){auto x=find(p);if(!x)throw std::runtime_error("Parent disappeared");if(reaches(r.uid,p,{}))throw std::runtime_error("Cycle detected: "+p+" -> "+r.uid);add(x->children,r.uid);route(x->type).update(*x);}for(const auto&c:r.children){auto x=find(c);if(!x)throw std::runtime_error("Child disappeared");if(reaches(c,r.uid,{}))throw std::runtime_error("Cycle detected: "+r.uid+" -> "+c);add(x->parents,r.uid);route(x->type).update(*x);}}
domain::Requirement RequirementCreationService::create(const domain::CreateRequirement&q){validate(q);auto r=route(q.type).create(q);link(r);return r;}
}
