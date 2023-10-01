#ifndef _WTSCLWQ_THREAD_
#define _WTSCLWQ_THREAD_

#include <functional>
#include <string>
#include <string_view>
#include "lock.h"

namespace wtsclwq {
class Thread : Noncopyable {
 public:
  /**
   * @brief 构造函数
   *
   * @param task_ 线程要执行的任务，构造成功后开始执行
   * @param name 线程名
   */
  Thread(std::function<void()> task_, std::string_view name);

  /**
   * @brief 析构函数
   */
  ~Thread() override;

  /**
   * @brief 获取Thread对象的线程id
   *
   * @return pid_t
   */
  auto GetId() const -> pid_t { return id_; }

  /**
   * @brief 获取Thread对象的线程名
   *
   * @return std::string
   */
  auto GetName() const -> std::string { return name_; }

  /**
   * @brief 等待Thread线程对象的任务执行完毕
   */
  void Join();

  /**
   * @brief 获取当前正在执行的线程的Thread指针
   *
   * @return Thread*
   */
  static auto GetCurrPtr() -> Thread *;

  /**
   * @brief 获取当前正在执行的线程的Threa对象的线程名
   *
   * @return std::string
   */
  static auto GetCurrName() -> std::string;

  /**
   * @brief 设置当前正在执行的线程的Thread对象的线程名
   *
   * @param name
   */
  static void SetCurrName(std::string_view name);

 private:
  /**
   * @brief Thread真正的主流程，作为pthread_create的参数
   *
   * @param args 用于传递this指针，因为this->task_需要在其中被执行
   */
  static auto RealProcess(void *args) -> void *;

  pid_t id_ = -1;               // 线程id
  pthread_t sys_thread_ = 0;    // pthread线程对象标识符，0表示未创建
  std::function<void()> task_;  // 线程要执行的任务
  std::string name_;            // 线程名
  Semaphore sem_;               // 信号量，用于阻塞构造函数，等待线程启动成功
};
}  // namespace wtsclwq

#endif