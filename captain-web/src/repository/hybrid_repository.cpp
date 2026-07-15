#include "captain/repository/hybrid_repository.hpp"
#include <map>
#include <stdexcept>
namespace captain::repository {
HybridRepository::HybridRepository(std::shared_ptr<storage::IRequirementStorage>l,std::shared_ptr<storage::IRequirementStorage>r,std::shared_ptr<storage::ICollaborationStorage>c):local_(std::move(l)),remote_(std::move(r)),collaboration_(std::move(c)){}
std::vector<domain::Requirement> HybridRepository::all()const{std::map<std::string,domain::Requirement>m;for(auto&r:local_->all()){if(r.authority!=domain::Authority::remote)m.emplace(r.uid,std::move(r));}for(auto&r:remote_->all()){if(m.contains(r.uid))throw std::runtime_error("UID conflict: "+r.uid);m.emplace(r.uid,std::move(r));}std::vector<domain::Requirement>v;for(auto&[uid,r]:m){(void)uid;v.push_back(std::move(r));}return v;}
domain::Requirement HybridRepository::create(const domain::CreateRequirement&q){if(local_->writable(q.type))return local_->create(q);if(remote_->writable(q.type))return remote_->create(q);throw std::runtime_error("No storage route for type "+q.type);}
std::vector<domain::Comment> HybridRepository::comments(const std::string&u)const{return collaboration_->comments(u);}domain::Comment HybridRepository::add_comment(const std::string&u,const std::string&a,const std::string&t){return collaboration_->add_comment(u,a,t);}std::vector<domain::FileLink> HybridRepository::files(const std::string&u)const{return collaboration_->files(u);}domain::FileLink HybridRepository::add_file(const domain::FileLink&f){return collaboration_->add_file(f);}
}
