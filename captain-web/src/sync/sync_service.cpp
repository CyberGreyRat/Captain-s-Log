#include "captain/sync/sync_service.hpp"
namespace captain::sync {SyncService::SyncService(storage::LocalYamlStorage&l,storage::IRequirementStorage&r):local_(l),remote_(r){}Result SyncService::pull(){Result x;for(auto&r:remote_.all()){local_.write_mirror(r);++x.pulled;}return x;}}
