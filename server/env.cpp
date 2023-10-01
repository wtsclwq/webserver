#include "env.h"
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <string>
#include "lock.h"
#include "log.h"

namespace wtsclwq {

static auto logger = NAMED_LOGGER("system");

auto EnvManager::Init(int argc, char **argv) -> bool {
  // 使用readlink获取程序绝对路径
  char path[1024]{};
  char buff[1024]{};
  sprintf(path, "/proc/%d/exe", getpid());
  int ret = readlink(path, buff, sizeof(path));
  if (ret == -1) {
    throw std::runtime_error("readlink error");
  }
  exe_abs_path_ = buff;
  auto pos = exe_abs_path_.find_last_of('/');
  pwd_ = exe_abs_path_.substr(0, pos) + "/";

  // 获取程序名称
  program_name_ = argv[0];

  // 解析命令行参数
  // ./exe -key1 value1 -key2 value2 -key3 -key4
  const char *now_key = nullptr;
  // 从1开始，跳过程序名
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (std::strlen(argv[i]) > 1) {
        // -key1 -key2的情况，先把上一个key的value设置为空字符串
        if (now_key != nullptr) {
          AddArg(now_key, "");
        }
        // 将当前key设置为新的key
        now_key = argv[i] + 1;
      } else {
        // -后面没有字符，解析出错
        LOG_ERROR(logger) << "parse args error idx = " << i << " arg = " << argv[i] << std::endl;
        return false;
      }
    } else {
      // -key1 value1
      if (now_key != nullptr) {
        AddArg(now_key, argv[i]);
        now_key = nullptr;
      } else {
        // 没有key，直接value，解析出错
        LOG_ERROR(logger) << "parse args error idx = " << i << " arg = " << argv[i] << std::endl;
        return false;
      }
    }
  }
  // 如果最后一个是key，那么value为空字符串
  if (now_key != nullptr) {
    AddArg(now_key, "");
  }
  return true;
}

void EnvManager::AddArg(std::string_view key, std::string_view value) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  args_.emplace(key, value);
}

auto EnvManager::GetArg(std::string_view key) -> std::string{
  std::shared_lock<std::shared_mutex> lock(mutex_);
  auto iter = args_.find(std::string(key));
  if (iter != args_.end()) {
    return iter->second;
  }
  return "";
}

auto EnvManager::CheckArg(std::string_view key) -> bool {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  return args_.count(std::string(key)) != 0;
}

void EnvManager::RemoveArg(std::string_view key) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  args_.erase(std::string(key));
}

auto EnvManager::GetWithDefaultArg(std::string_view key, std::string_view default_value) -> std::string {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  auto iter = args_.find(std::string(key));
  if (iter != args_.end()) {
    return iter->second;
  }
  return std::string(default_value);
}

void EnvManager::AddHelp(std::string_view key, std::string_view value) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  helps_.emplace_back(key, value);
}

void EnvManager::RemoveHelp(std::string_view key) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  for (auto iter = helps_.begin(); iter != helps_.end(); iter++) {
    if (iter->first == key) {
      helps_.erase(iter);
      break;
    }
  }
}

void EnvManager::PrintHelps() {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  std::cout << "Usage: " << program_name_ << " [options]" << std::endl;
  std::cout << "Options:" << std::endl;
  for (auto &help : helps_) {
    std::cout << std::setw(10) << std::left << help.first << " " << help.second << std::endl;
  }
}

auto EnvManager::SetEnv(std::string_view key, std::string_view value) -> bool {
  return setenv(key.data(), value.data(), 1) == 0;
}

auto EnvManager::GetEnvWithDefault(std::string_view key, std::string_view default_value) -> std::string {
  auto value = getenv(key.data());
  if (value == nullptr) {
    return std::string(default_value);
  }
  return value;
}

auto EnvManager::GetExeAbsPath() const -> std::string { return exe_abs_path_; }

auto EnvManager::GetProgramName() const -> std::string { return program_name_; }

auto EnvManager::GetPwd() const -> std::string { return pwd_; }

auto EnvManager::GetAbsoluteSubPath(std::string_view sub_path) const -> std::string {
  if (sub_path.empty()) {
    return pwd_;
  }
  if (sub_path[0] == '/') {
    return std::string(sub_path);
  }
  return pwd_ + std::string(sub_path);
}

auto EnvManager::GetConfigPath() -> std::string { return GetWithDefaultArg("c", "config"); }
}  // namespace wtsclwq