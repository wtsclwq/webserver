#ifndef _WTSCLWQ_FD_MANAGER_
#define _WTSCLWQ_FD_MANAGER_

#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <vector>
#include "server/singleton.h"

namespace wtsclwq {

class FileInfoWrapper : public std::enable_shared_from_this<FileInfoWrapper> {
 public:
  using s_ptr = std::shared_ptr<FileInfoWrapper>;

  explicit FileInfoWrapper(int fd);

  ~FileInfoWrapper();

  /**
   * @brief 是否初始化完成
   */
  auto IsInited() -> bool;

  /**
   * @brief 是否是socket
   */
  auto IsSocket() -> bool;

  /**
   * @brief 是否已经关闭
   */
  auto IsClosed() -> bool;

  /**
   * @brief 用户手动设置为非阻塞模式
   */
  void SetUserLevelNonBlock(bool v);

  /**
   * @brief 获取是否为用户手动设置的非阻塞模式
   */
  auto IsUserLevelNonBlock() -> bool;

  /**
   * @brief 系统设置为非阻塞模式
   */
  void SetSysLevelNonBlock(bool v);

  /**
   * @brief 获取是否为系统默认的非阻塞模式
   */
  auto IsSysLevelNonBlock() -> bool;

  /**
   * @brief 设置fd上的IO超时时间
   */
  void SetTimeout(int type, uint64_t timeout_ms);

  /**
   * @brief 获取fd上的IO超时时间
   */
  auto GetTimeout(int type) -> uint64_t;

 private:
  /**
   * @brief 初始化fd
   */
  auto Init() -> bool;

  int sys_fd_{0};                          // 系统fd
  bool is_inited_{false};                  // 是否初始化完成
  bool is_socket_{false};                  // 是否是socket
  bool is_closed_{false};                  // 是否已经关闭
  bool is_user_non_block_{false};          // 用户手动设置为非阻塞模式
  bool is_sys_non_block_{false};           // 系统设置为非阻塞模式
  uint64_t read_timeout_ms_{UINT64_MAX};   // 读超时时间
  uint64_t write_timeout_ms_{UINT64_MAX};  // 写超时时间
};

class FileInfoWrapperManager {
 public:
  using s_ptr = std::shared_ptr<FileInfoWrapperManager>;
  using MutexType = std::shared_mutex;

  FileInfoWrapperManager();

  ~FileInfoWrapperManager();

  /**
   * @brief 获取fd对应的FileInfoWrapper
   */
  auto Get(int fd, bool auto_create = false) -> FileInfoWrapper::s_ptr;

  /**
   * @brief 删除fd对应的FileInfoWrapper
   */
  void Remove(int fd);

 private:
  std::vector<FileInfoWrapper::s_ptr> fd_infos_{};  // fd对应的FileInfoWrapper
  MutexType mutex_{};                               // 互斥锁
};

/**
 * @brief 单例模式
 */
using FdWrapperMgr = Singleton<FileInfoWrapperManager>;
}  // namespace wtsclwq

#endif  // _WTSCLWQ_FD_MANAGER_