#ifndef _WTSCLWQ_ENV_
#define _WTSCLWQ_ENV_

#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "lock.h"
#include "singleton.h"

namespace wtsclwq {
class EnvManager {
 public:
  using MutexType = std::shared_mutex;

  /**
   * @brief 初始化，包括记录程序名称与路径，解析命令行选项和参数
   * @details 命令行选项全部以 - 开头，后根可选参数，选项与参数构成k-v数据结构，如果只有key没有value则value为空字符串
   * @param argc 命令行参数个数
   * @param argv 命令行参数数组
   * @return bool 是否初始化成功
   */
  auto Init(int argc, char **argv) -> bool;

  /**
   * @brief 添加自定义参数
   * @param key
   * @param value
   */
  void AddArg(std::string_view key, std::string_view value);

  /**
   * @brief 获取参数值
   * @param key
   * @return std::string
   */
  auto GetArg(std::string_view key) -> std::string;

  /**
   * @brief 检查是否存在某个参数
   * @param key
   * @return true
   * @return false
   */
  auto CheckArg(std::string_view key) -> bool;

  /**
   * @brief 删除某个参数
   * @param key
   */
  void RemoveArg(std::string_view key);

  /**
   * @brief 获取某个参数的值，如果没找到，则返回调用时提供的默认值
   *
   * @param key
   * @return std::string
   */
  auto GetWithDefaultArg(std::string_view key, std::string_view default_value) -> std::string;

  /**
   * @brief 增加命令行帮助选项
   * @param key
   * @param value
   */
  void AddHelp(std::string_view key, std::string_view value);

  /**
   * @brief 删除某个命令行帮助选项
   * @param key
   */
  void RemoveHelp(std::string_view key);

  /**
   * @brief 打印帮助信息
   */
  void PrintHelps();

  auto GetExeAbsPath() const -> std::string;

  auto GetProgramName() const -> std::string;

  auto GetPwd() const -> std::string;

  /**
   * @brief 设置系统环境变量
   *
   * @param key
   * @param value
   * @return true
   * @return false
   */
  auto SetEnv(std::string_view key, std::string_view value) -> bool;

  /**
   * @brief 获取系统环境变量，如果没找到，则返回调用时提供的默认值
   *
   * @param key
   * @param default_value
   * @return std::string
   */
  auto GetEnvWithDefault(std::string_view key, std::string_view default_value) -> std::string;

  /**
   * @brief 获取给定的path的绝对路径
   * @param sub_path 可能是一个绝对路径，也可能是相对于当前工作目录的相对路径
   * @return std::string
   */
  auto GetAbsoluteSubPath(std::string_view sub_path) const -> std::string;

  /**
   * @brief 获取配置文件路径，配置文件路径通过-c选项指定，默认为当前工作目录下的config文件夹
   */
  auto GetConfigPath() -> std::string;

 private:
  std::unordered_map<std::string, std::string> args_{};       // 命令行参数
  std::vector<std::pair<std::string, std::string>> helps_{};  // 帮助信息
  std::string program_name_{};                                // 程序名(argv[0])
  std::string exe_abs_path_{};                                // 绝对路径 /proc/%pid/exe软链接指定的路径
  std::string pwd_{};                                         // 当前工作目录，可以从argv[0]中获取
  MutexType mutex_{};
};

using EnvMgr = SingletonPtr<EnvManager>;
}  // namespace wtsclwq

#endif  // _WTSCLWQ_ENV_