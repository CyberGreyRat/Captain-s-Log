#pragma once
#include "captain/storage/local_yaml_storage.hpp"
namespace captain::sync {struct Result{std::size_t pulled{},unchanged{};};class SyncService{public:SyncService(storage::LocalYamlStorage&,storage::IRequirementStorage&);Result pull();private:storage::LocalYamlStorage& local_;storage::IRequirementStorage& remote_;};}
