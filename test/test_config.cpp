#include "server/config.h"
#include "server/env.h"

auto root_logger = ROOT_LOGGER;

auto g_int = wtsclwq::ConfigMgr::GetInstance()->GetOrAddDefaultConfigItem("global.int", 8080, "global int");
auto g_float = wtsclwq::ConfigMgr::GetInstance()->GetOrAddDefaultConfigItem("global.float", 1.23F, "global float");
auto g_string =
    wtsclwq::ConfigMgr::GetInstance()->GetOrAddDefaultConfigItem("global.string", std::string("hello world"), "global string");
auto g_list = wtsclwq::ConfigMgr::GetInstance()->GetOrAddDefaultConfigItem("global.list", std::list{1, 2, 3}, "global list");
auto g_set = wtsclwq::ConfigMgr::GetInstance()->GetOrAddDefaultConfigItem("global.set", std::set{1, 2, 3}, "global set");
auto g_unordered_set = wtsclwq::ConfigMgr::GetInstance()->GetOrAddDefaultConfigItem("global.unordered_set", std::set{1, 2, 3},
                                                                         "global unordered_set");
auto g_map = wtsclwq::ConfigMgr::GetInstance()->GetOrAddDefaultConfigItem(
    "global.map", std::map<std::string, int>{{"key1", 1}, {"key2", 3}, {"key3", 3}}, "global map");
auto g_unordered_map = wtsclwq::ConfigMgr::GetInstance()->GetOrAddDefaultConfigItem(
    "global.unordered_map", std::unordered_map<std::string, int>{{"key1", 1}, {"key2", 3}, {"key3", 3}},
    "global unordered_map");
auto g_vector =
    wtsclwq::ConfigMgr::GetInstance()->GetOrAddDefaultConfigItem("global.vector", std::vector{1, 2, 3}, "global vector");

class Person {
 public:
  Person() = default;
  std::string name_;
  int age_ = 0;
  bool sex_ = false;

  auto ToString() const -> std::string {
    std::stringstream ss;
    ss << "[Person name=" << name_ << " age=" << age_ << " sex=" << sex_ << "]";
    return ss.str();
  }

  auto operator==(const Person &oth) const -> bool {
    return name_ == oth.name_ && age_ == oth.age_ && sex_ == oth.sex_;
  }
};

namespace wtsclwq {
template <>
class LexicalCast<std::string, Person> {
 public:
  auto operator()(const std::string &v) -> Person {
    YAML::Node node = YAML::Load(v);
    Person p;
    p.name_ = node["name"].as<std::string>();
    p.age_ = node["age"].as<int>();
    p.sex_ = node["sex"].as<bool>();
    return p;
  }
};

template <>
class LexicalCast<Person, std::string> {
 public:
  auto operator()(const Person &p) -> std::string {
    YAML::Node node;
    node["name"] = p.name_;
    node["age"] = p.age_;
    node["sex"] = p.sex_;
    std::stringstream ss;
    ss << node;
    return ss.str();
  }
};
}  // namespace wtsclwq

auto g_person = wtsclwq::ConfigMgr::GetInstance()->GetOrAddDefaultConfigItem("global.person", Person(), "global person");

auto g_person_map = wtsclwq::ConfigMgr::GetInstance()->GetOrAddDefaultConfigItem(
    "global.person_map", std::map<std::string, Person>{{}}, "global person map");

auto g_person_vec_map = wtsclwq::ConfigMgr::GetInstance()->GetOrAddDefaultConfigItem(
    "global.person_vec_map", std::map<std::string, std::vector<Person>>{{}}, "global person vec map");

void TestClass() {
  static uint64_t id = 0;

  if (!g_person->GetListener(id)) {
    id = g_person->AddListener([](const Person &old_value, const Person &new_value) {
      LOG_INFO(root_logger) << "g_person value change, old value:" << old_value.ToString()
                            << ", new value:" << new_value.ToString();
    });
  }

  LOG_INFO(root_logger) << g_person->GetValue().ToString();

  for (const auto &i : g_person_map->GetValue()) {
    LOG_INFO(root_logger) << i.first << ":" << i.second.ToString();
  }

  for (const auto &i : g_person_vec_map->GetValue()) {
    LOG_INFO(root_logger) << i.first;
    for (const auto &j : i.second) {
      LOG_INFO(root_logger) << j.ToString();
    }
  }
}

template <class T>
auto FormatArray(const T &v) -> std::string {
  std::stringstream ss;
  ss << "[";
  for (const auto &i : v) {
    ss << " " << i;
  }
  ss << " ]";
  return ss.str();
}

template <class T>
auto FormatMap(const T &m) -> std::string {
  std::stringstream ss;
  ss << "{";
  for (const auto &i : m) {
    ss << " {" << i.first << ":" << i.second << "}";
  }
  ss << " }";
  return ss.str();
}

void TestConfig() {
  LOG_INFO(root_logger) << "g_int value: " << g_int->GetValue();
  LOG_INFO(root_logger) << "g_float value: " << g_float->GetValue();
  LOG_INFO(root_logger) << "g_string value: " << g_string->GetValue();
  LOG_INFO(root_logger) << "g_int_vec value: " << FormatArray<std::vector<int>>(g_vector->GetValue());
  LOG_INFO(root_logger) << "g_int_list value: " << FormatArray<std::list<int>>(g_list->GetValue());
  LOG_INFO(root_logger) << "g_int_set value: " << FormatArray<std::set<int>>(g_set->GetValue());
  LOG_INFO(root_logger) << "g_int_map value: " << FormatMap<std::map<std::string, int>>(g_map->GetValue());
  LOG_INFO(root_logger) << "g_int_unordered_map value: "
                        << FormatMap<std::unordered_map<std::string, int>>(g_unordered_map->GetValue());

  // 自定义配置项
  TestClass();
}

auto main(int argc, char **argv) -> int {
  // 设置g_int的配置变更回调函数
  g_int->AddListener([](const int &old_value, const int &new_value) {
    LOG_INFO(root_logger) << "g_int value changed, old_value: " << old_value << ", new_value: " << new_value;
  });

  LOG_INFO(root_logger) << "before============================";

  TestConfig();

  // 从配置文件中加载配置，由于更新了配置，会触发配置项的配置变更回调函数
  wtsclwq::EnvMgr::GetInstance()->Init(argc, argv);
  wtsclwq::ConfigMgr::GetInstance()->LoadFromConfDir(wtsclwq::EnvMgr::GetInstance()->GetConfigPath());
  LOG_INFO(root_logger) << "after============================";

  TestConfig();

  // 遍历所有配置
  wtsclwq::ConfigMgr::GetInstance()->Visit([](const wtsclwq::ConfigItemBase::s_ptr &var) {
    LOG_INFO(root_logger) << "name=" << var->GetName() << " description=" << var->GetDescription()
                          << " typename=" << var->GetType() << " value=" << var->ToString();
  });

  return 0;
}