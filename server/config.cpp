#include "config.h"
#include "env.h"
#include "log.h"
#include "utils.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <shared_mutex>

namespace wtsclwq {
static Logger::s_ptr logger = NAMED_LOGGER("system");

ConfigItemBase::ConfigItemBase(std::string_view name, std::string_view description)
    : name_(name), description_(description) {
  std::transform(name_.begin(), name_.end(), name_.begin(), ::tolower);
}

ConfigItemBase::~ConfigItemBase() = default;

auto ConfigItemBase::GetName() const -> std::string { return name_; }

auto ConfigItemBase::GetDescription() const -> std::string { return description_; }

auto ConfigManager::GetConfigItemBase(const std::string &name) -> ConfigItemBase::s_ptr {
  std::shared_lock<MutexType> lock(GetMutex());
  auto &dict = GetConfigDict();
  auto it = dict.find(name);
  if (it == dict.end()) {
    return nullptr;
  }
  return it->second;
}

/**
 * @brief 遍历所有配置项，组织成一个list，<parent1.parent2.child, val>
 * @param prefix
 * @param node
 * @param output
 */
static void ListAllMember(std::string_view prefix, const YAML::Node &node,
                          std::list<std::pair<std::string, const YAML::Node>> *output) {
  const std::string valid_prefix{"abcdefghijklmnopqrstuvwxyz._0123456789"};
  if (prefix.find_first_not_of(valid_prefix) != std::string::npos) {
    LOG_ERROR(logger) << "Config invalid name: " << prefix << ":" << node << "\n";
    return;
  }
  output->emplace_back(prefix, node);
  if (node.IsMap()) {
    for (auto it = node.begin(); it != node.end(); ++it) {
      ListAllMember(prefix.empty() ? it->first.Scalar() : std::string(prefix) + "." + it->first.Scalar(), it->second,
                    output);
    }
  }
}

void ConfigManager::LoadFromYaml(const YAML::Node &root) {
  std::list<std::pair<std::string, const YAML::Node>> all_nodes;
  ListAllMember("", root, &all_nodes);
  for (auto &i : all_nodes) {
    auto key = i.first;
    if (key.empty()) {
      continue;
    }
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);

    ConfigItemBase::s_ptr item = GetConfigItemBase(key);
    if (item == nullptr) {
      continue;
    }
    if (i.second.IsScalar()) {
      item->FromString(i.second.Scalar());
    } else {
      std::stringstream ss;
      ss << i.second;
      item->FromString(ss.str());
    }
  }
}

static std::unordered_map<std::string, u_int64_t> file2modifytime;
static std::mutex mutex;

void ConfigManager::LoadFromConfDir(std::string_view path, bool force) {
  std::string abs_path = EnvMgr::GetInstance()->GetAbsoluteSubPath(path);

  LOG_INFO(logger) << "Load conf dir: " << abs_path << "\n";

  std::vector<std::string> files;
  FSUtil::ListAllFile(&files, abs_path, ".yml");
  for (auto &i : files) {
    {
      struct stat st;
      std::lock_guard<std::mutex> lock(mutex);
      lstat(i.c_str(), &st);
      if (!force && file2modifytime[i] == st.st_mtime) {
        continue;
      }
      file2modifytime[i] = st.st_mtime;
    }
    try {
      YAML::Node root = YAML::LoadFile(i);
      LoadFromYaml(root);
      LOG_INFO(logger) << "Load conf file: " << i << " ok\n";
    } catch (...) {
      LOG_ERROR(logger) << "Load conf file: " << i << " failed\n";
    }
  }
}

void ConfigManager::Visit(const std::function<void(ConfigItemBase::s_ptr)> &cb) {
  std::shared_lock<MutexType> lock(GetMutex());
  auto &dict = GetConfigDict();
  for (auto &i : dict) {
    cb(i.second);
  }
}

auto ConfigManager::GetConfigDict() -> ConfigItemDict & {
  static ConfigItemDict config_dict;
  return config_dict;
}

auto ConfigManager::GetMutex() -> MutexType & {
  static MutexType mutex;
  return mutex;
}

}  // namespace wtsclwq