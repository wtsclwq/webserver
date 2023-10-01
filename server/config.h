#ifndef _WTSCLWQ_CONFIG_
#define _WTSCLWQ_CONFIG_

#include <yaml-cpp/node/node.h>
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <cstdint>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "log.h"

namespace wtsclwq {
class ConfigItemBase {
 public:
  using s_ptr = std::shared_ptr<ConfigItemBase>;
  explicit ConfigItemBase(std::string_view name, std::string_view description = "");

  virtual ~ConfigItemBase();

  auto GetName() const -> std::string;

  auto GetDescription() const -> std::string;

  /**
   * @brief 将配置项转换成字符串
   * @return std::string
   */
  virtual auto ToString() -> std::string = 0;

  /**
   * @brief 从字符串初始化值
   * @param str 字符串值
   * @return true 初始化成功
   * @return false 初始化失败
   */
  virtual auto FromString(const std::string &str) -> bool = 0;

  /**
   * @brief 获取配置参数的类型名称
   * @return std::string
   */
  virtual auto GetType() const -> std::string = 0;

 protected:
  std::string name_{};
  std::string description_{};
};

/**
 * @brief 类型转换模板仿函数类，将From类型转换成To类型
 * @tparam From
 * @tparam To
 */
template <typename From, typename To>
class LexicalCast {
 public:
  auto operator()(const From &value) -> To;
};

/**
 * @brief 配置项类，继承自配置项基类
 * @tparam T 配置项值的类型
 * @tparam FromStr 类型转换模板仿函数类，将string转换成T类型配置值
 * @tparam ToStr 类型转换模板仿函数类，将T类型配置值转换成string
 * @tparam std::string>
 */
template <typename T, typename FromStr = LexicalCast<std::string, T>, typename ToStr = LexicalCast<T, std::string>>
class ConfigItem : public ConfigItemBase {
 public:
  using s_ptr = std::shared_ptr<ConfigItem>;
  using MutexType = std::shared_mutex;
  using OnChangeCallback = std::function<void(const T &old_value, const T &new_value)>;

  /**
   * @brief 通过参数名、默认值、描述构造配置项
   * @param name 有效名称仅支持[0-9a-z_.]
   * @param default_value 默认值
   * @param description 描述
   */
  ConfigItem(std::string_view name, const T &default_value, std::string_view description = "");

  /**
   * @brief 转换成yaml字符串
   * @return std::string
   */
  auto ToString() -> std::string override;

  /**
   * @brief 从yaml字符串初始化自身
   * @return 是否成功
   */
  auto FromString(const std::string &str) -> bool override;

  /**
   * @brief 返回配置项的类型名称
   */
  auto GetType() const -> std::string override;

  /**
   * @brief 获取配置项的值
   */
  auto GetValue() const -> T;

  /**
   * @brief 设置配置项的值
   */
  void SetValue(const T &value);

  /**
   * @brief 添加配置项变化回调函数
   * @return 返回该回调函数的唯一id，用于删除该回调函数
   */
  auto AddListener(OnChangeCallback cb) -> uint64_t;

  /**
   * @brief 删除配置项变化回调函数
   */
  void DelListener(uint64_t key);

  /**
   * @brief 根据key获取配置项变化回调函数
   */
  auto GetListener(uint64_t key) -> OnChangeCallback;

  /**
   * @brief 清空配置项变化回调函数
   */
  void ClearListener();

 private:
  T val_{};
  std::unordered_map<uint64_t, OnChangeCallback> cbs_{};
  mutable MutexType mutex_{};
};

/**
 * @brief 管理所有的配置项
 */
class ConfigManager {
 public:
  using ConfigItemDict = std::unordered_map<std::string, ConfigItemBase::s_ptr>;
  using MutexType = std::shared_mutex;

  /**
   * @brief 获取配置项
   * @tparam T 配置项类型
   * @param name 配置项名称
   * @return 配置项
   */
  template <typename T>
  static auto GetConfigItem(const std::string &name) -> typename ConfigItem<T>::s_ptr;

  /**
   * @brief 查找或者创建一个配置项
   * @param name 配置项名称
   * @param default_value 配置项值（查找失败时使用）
   * @param description 配置项描述（查找失败时使用）
   * @return ConfigItem<T>::s_ptr
   */
  template <typename T>
  static auto GetOrAddDefaultConfigItem(const std::string &name, const T &default_value,
                                        const std::string &description = "") -> typename ConfigItem<T>::s_ptr;

  /**
   * @brief 使用YAML::Node初始化配置项
   */
  static void LoadFromYaml(const YAML::Node &root);

  /**
   * @brief 从path文件夹中加载配置文件
   */
  static void LoadFromConfDir(std::string_view path, bool force = false);

  /**
   * @brief 查找配置项，返回配置项的基类指针
   */
  static auto GetConfigItemBase(const std::string &name) -> ConfigItemBase::s_ptr;

  /**
   * @brief 遍历所有配置项，对其使用回调函数
   */
  static void Visit(const std::function<void(ConfigItemBase::s_ptr)> &cb);

 private:
  static auto GetConfigDict() -> ConfigItemDict &;

  static auto GetMutex() -> MutexType &;
};

/**
 * @brief 偏特化，将yaml字符串转换成std::vector<T>
 */
template <typename T>
class LexicalCast<std::string, std::vector<T>> {
 public:
  auto operator()(const std::string &value) -> std::vector<T>;
};

/**
 * @brief 偏特化，将std::vector<T>转换成yaml字符串
 */
template <typename T>
class LexicalCast<std::vector<T>, std::string> {
 public:
  auto operator()(const std::vector<T> &value) -> std::string;
};

/**
 * @brief 偏特化，将yaml字符串转换成std::list<T>
 */
template <typename T>
class LexicalCast<std::string, std::list<T>> {
 public:
  auto operator()(const std::string &value) -> std::list<T>;
};

/**
 * @brief 偏特化，将std::list<T>转换成yaml字符串
 */
template <typename T>
class LexicalCast<std::list<T>, std::string> {
 public:
  auto operator()(const std::list<T> &value) -> std::string;
};

/**
 * @brief 偏特化，将yaml字符串转换成std::set<T>
 */
template <typename T>
class LexicalCast<std::string, std::set<T>> {
 public:
  auto operator()(const std::string &value) -> std::set<T>;
};

/**
 * @brief 偏特化，将std::set<T>转换成yaml字符串
 */
template <typename T>
class LexicalCast<std::set<T>, std::string> {
 public:
  auto operator()(const std::set<T> &value) -> std::string;
};

/**
 * @brief 偏特化，将yaml字符串转换成std::unordered_set<T>
 */
template <typename T>
class LexicalCast<std::string, std::unordered_set<T>> {
 public:
  auto operator()(const std::string &value) -> std::unordered_set<T>;
};

/**
 * @brief 偏特化，将std::unordered_set<T>转换成yaml字符串
 */
template <typename T>
class LexicalCast<std::unordered_set<T>, std::string> {
 public:
  auto operator()(const std::unordered_set<T> &value) -> std::string;
};

/**
 * @brief 偏特化，将yaml字符串转换成std::map<std::string, T>
 */
template <typename T>
class LexicalCast<std::string, std::map<std::string, T>> {
 public:
  auto operator()(const std::string &value) -> std::map<std::string, T>;
};

/**
 * @brief 偏特化，将std::map<std::string, T>转换成yaml字符串
 */
template <typename T>
class LexicalCast<std::map<std::string, T>, std::string> {
 public:
  auto operator()(const std::map<std::string, T> &value) -> std::string;
};

/**
 * @brief 偏特化，将yaml字符串转换成std::unordered_map<std::string, T>
 */
template <typename T>
class LexicalCast<std::string, std::unordered_map<std::string, T>> {
 public:
  auto operator()(const std::string &value) -> std::unordered_map<std::string, T>;
};

/**
 * @brief 偏特化，将std::unordered_map<std::string, T>转换成yaml字符串
 */
template <typename T>
class LexicalCast<std::unordered_map<std::string, T>, std::string> {
 public:
  auto operator()(const std::unordered_map<std::string, T> &value) -> std::string;
};

/***************************************************************************************/
/***************************************************************************************/
/***************************************************************************************/

template <typename T, typename FromStr, typename ToStr>
ConfigItem<T, FromStr, ToStr>::ConfigItem(std::string_view name, const T &default_value, std::string_view description)
    : ConfigItemBase(name, description), val_(default_value) {}

template <typename T, typename FromStr, typename ToStr>
auto ConfigItem<T, FromStr, ToStr>::ToString() -> std::string {
  try {
    // 读锁
    std::shared_lock<MutexType> lock(mutex_);
    return ToStr()(val_);
  } catch (const std::exception &e) {
    LOG_ERROR(ROOT_LOGGER) << "ConfigVar::toString exception " << e.what() << " convert: " << TypeToName<T>()
                           << " to string"
                           << " name=" << name_ << std::endl;
  }
  return "";
}

template <typename T, typename FromStr, typename ToStr>
auto ConfigItem<T, FromStr, ToStr>::FromString(const std::string &str) -> bool {
  try {
    SetValue(FromStr()(str));
  } catch (const std::exception &e) {
    LOG_ERROR(ROOT_LOGGER) << "ConfigVar::fromString exception " << e.what() << " convert: string to "
                           << TypeToName<T>() << " name=" << name_ << " - " << str << std::endl;
    return false;
  }
  return true;
}

template <typename T, typename FromStr, typename ToStr>
auto ConfigItem<T, FromStr, ToStr>::GetType() const -> std::string {
  return typeid(T).name();
}

template <typename T, typename FromStr, typename ToStr>
auto ConfigItem<T, FromStr, ToStr>::GetValue() const -> T {
  // 读锁
  std::shared_lock<MutexType> lock(mutex_);
  return val_;
}

template <typename T, typename FromStr, typename ToStr>
void ConfigItem<T, FromStr, ToStr>::SetValue(const T &value) {
  {
    // 读锁
    std::shared_lock<MutexType> lock(mutex_);
    if (val_ == value) {
      return;
    }
  }
  // 写锁
  std::lock_guard<MutexType> lock(mutex_);
  // 发生变动，调用所有回调函数
  for (auto &it : cbs_) {
    it.second(val_, value);
  }
  val_ = value;
}

template <typename T, typename FromStr, typename ToStr>
auto ConfigItem<T, FromStr, ToStr>::AddListener(OnChangeCallback cb) -> uint64_t {
  // key从0开始，每次加1
  static uint64_t key = 0;
  // 写锁
  std::lock_guard<MutexType> lock(mutex_);
  ++key;
  cbs_.emplace(key, cb);
  return key;
}

template <typename T, typename FromStr, typename ToStr>
void ConfigItem<T, FromStr, ToStr>::DelListener(uint64_t key) {
  // 写锁
  std::lock_guard<MutexType> lock(mutex_);
  cbs_.erase(key);
}

template <typename T, typename FromStr, typename ToStr>
auto ConfigItem<T, FromStr, ToStr>::GetListener(uint64_t key) -> OnChangeCallback {
  // 读锁
  std::shared_lock<MutexType> lock(mutex_);
  auto it = cbs_.find(key);
  return it == cbs_.end() ? nullptr : it->second;
}

template <typename T, typename FromStr, typename ToStr>
void ConfigItem<T, FromStr, ToStr>::ClearListener() {
  // 写锁
  std::lock_guard<MutexType> lock(mutex_);
  cbs_.clear();
}

template <typename From, typename To>
auto LexicalCast<From, To>::operator()(const From &value) -> To {
  return boost::lexical_cast<To>(value);
}

template <typename T>
auto LexicalCast<std::string, std::vector<T>>::operator()(const std::string &value) -> std::vector<T> {
  YAML::Node node = YAML::Load(value);
  typename std::vector<T> vec;
  std::stringstream ss;
  for (auto &&i : node) {
    ss.str("");
    ss << i;
    vec.emplace_back(LexicalCast<std::string, T>()(ss.str()));
  }
  return vec;
}

template <typename T>
auto LexicalCast<std::vector<T>, std::string>::operator()(const std::vector<T> &value) -> std::string {
  YAML::Node node(YAML::NodeType::Sequence);
  for (auto &&i : value) {
    node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

template <typename T>
auto LexicalCast<std::string, std::list<T>>::operator()(const std::string &value) -> std::list<T> {
  YAML::Node node = YAML::Load(value);
  typename std::list<T> res_list;
  std::stringstream ss;
  for (auto &&i : node) {
    ss.str("");
    ss << i;
    res_list.emplace_back(LexicalCast<std::string, T>()(ss.str()));
  }
  return res_list;
}

template <typename T>
auto LexicalCast<std::list<T>, std::string>::operator()(const std::list<T> &value) -> std::string {
  YAML::Node node(YAML::NodeType::Sequence);
  for (auto &&i : value) {
    node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

template <typename T>
auto LexicalCast<std::string, std::set<T>>::operator()(const std::string &value) -> std::set<T> {
  YAML::Node node = YAML::Load(value);
  typename std::set<T> res_set;
  std::stringstream ss;
  for (auto &&i : node) {
    ss.str("");
    ss << i;
    res_set.emplace(LexicalCast<std::string, T>()(ss.str()));
  }
  return res_set;
}

template <typename T>
auto LexicalCast<std::set<T>, std::string>::operator()(const std::set<T> &value) -> std::string {
  YAML::Node node(YAML::NodeType::Sequence);
  for (auto &&i : value) {
    node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

template <typename T>
auto LexicalCast<std::string, std::unordered_set<T>>::operator()(const std::string &value) -> std::unordered_set<T> {
  YAML::Node node = YAML::Load(value);
  typename std::unordered_set<T> res_set;
  std::stringstream ss;
  for (auto &&i : node) {
    ss.str("");
    ss << i;
    res_set.emplace(LexicalCast<std::string, T>()(ss.str()));
  }
  return res_set;
}

template <typename T>
auto LexicalCast<std::unordered_set<T>, std::string>::operator()(const std::unordered_set<T> &value) -> std::string {
  YAML::Node node(YAML::NodeType::Sequence);
  for (auto &&i : value) {
    node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

template <typename T>
auto LexicalCast<std::string, std::map<std::string, T>>::operator()(const std::string &value)
    -> std::map<std::string, T> {
  YAML::Node node = YAML::Load(value);
  typename std::map<std::string, T> res_map;
  std::stringstream ss;
  for (auto &&i : node) {
    ss.str("");
    ss << i.second;
    res_map.emplace(i.first.Scalar(), LexicalCast<std::string, T>()(ss.str()));
  }
  return res_map;
}

template <typename T>
auto LexicalCast<std::map<std::string, T>, std::string>::operator()(const std::map<std::string, T> &value)
    -> std::string {
  YAML::Node node(YAML::NodeType::Sequence);
  for (auto &&i : value) {
    node.push_back(YAML::Load(i.first + ":" + LexicalCast<T, std::string>()(i.second)));
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

template <typename T>
auto LexicalCast<std::string, std::unordered_map<std::string, T>>::operator()(const std::string &value)
    -> std::unordered_map<std::string, T> {
  YAML::Node node = YAML::Load(value);
  typename std::unordered_map<std::string, T> res_map;
  std::stringstream ss;
  for (auto &&i : node) {
    ss.str("");
    ss << i.second;
    res_map.emplace(i.first.Scalar(), LexicalCast<std::string, T>()(ss.str()));
  }
  return res_map;
}

template <typename T>
auto LexicalCast<std::unordered_map<std::string, T>, std::string>::operator()(
    const std::unordered_map<std::string, T> &value) -> std::string {
  YAML::Node node(YAML::NodeType::Sequence);
  for (auto &&i : value) {
    node.push_back(YAML::Load(i.first + ":" + LexicalCast<T, std::string>()(i.second)));
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

template <typename T>
auto ConfigManager::GetConfigItem(const std::string &name) -> typename ConfigItem<T>::s_ptr {
  // 读锁
  std::shared_lock<MutexType> lock(GetMutex());
  auto it = GetConfigDict().find(name);
  if (it == GetConfigDict().end()) {
    return nullptr;
  }
  // 利用dynamic_pointer_cast进行智能指针的类型转换，多态，转换失败返回nullptr
  auto res = std::dynamic_pointer_cast<ConfigItem<T>>(it->second);
  if (res == nullptr) {
    LOG_ERROR(ROOT_LOGGER) << "Lookup name=" << name << " exists but type not " << TypeToName<T>()
                           << " real_type=" << it->second->GetType() << " " << it->second->ToString() << std::endl;
    return nullptr;
  }

  LOG_INFO(ROOT_LOGGER) << "Lookup name=" << name << " exist" << std::endl;
  return res;
}

template <typename T>
auto ConfigManager::GetOrAddDefaultConfigItem(const std::string &name, const T &default_value,
                                              const std::string &description) -> typename ConfigItem<T>::s_ptr {
  auto item = GetConfigItem<T>(name);
  if (item) {
    return item;
  }
  // 写锁
  std::lock_guard<MutexType> lock(GetMutex());
  if (name.find_first_not_of("abcdefghikjlmnopqrstuvwxyz._012345678") != std::string::npos) {
    LOG_ERROR(ROOT_LOGGER) << "Lookup name invalid " << name;
    throw std::invalid_argument(name);
  }

  item = std::make_shared<ConfigItem<T>>(name, default_value, description);
  GetConfigDict().emplace(name, item);
  return item;
}

}  // namespace wtsclwq

#endif  // _WTSCLWQ_CONFIG_