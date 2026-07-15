#pragma once
#include "captain/storage/local_yaml_storage.hpp"
namespace captain::sync {struct Result{std::size_t pulled{},pushed{},unchanged{};};class SyncService{public:SyncService(storage::LocalYamlStorage&,storage::IRequirementStorage&,storage::ISnapshotStorage&);Result pull();Result push(const std::string&git_commit,const std::string&actor);private:storage::LocalYamlStorage&local_;storage::IRequirementStorage&remote_;storage::ISnapshotStorage&snapshots_;};}
