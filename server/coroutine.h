#ifndef _WTSCLWQ_COROUTINE_
#define _WTSCLWQ_COROUTINE_

#include <sys/types.h>
#include <ucontext.h>
#include <cstdint>
#include <functional>
#include <memory>
#include "thread.h"

namespace wtsclwq {
class Coroutine : public std::enable_shared_from_this<Coroutine> {
 public:
  enum State {
    Ready,
    Running,
    Stop,
  };

  using s_ptr = std::shared_ptr<Coroutine>;

  /**
   * @brief 构造函数
   * @param task 协程要执行的具体任务
   * @param stack_size 协程栈大小
   * @param if_run_in_scheduler 构造函数所在线程是否参与协程调度
   */
  explicit Coroutine(std::function<void()> task_func, uint32_t stack_size = 0, bool has_parent = false, const s_ptr &parent = nullptr);

  ~Coroutine();

  /**
   * @brief 更新协程的具体任务，达到复用协程对象的目的
   * @param new_task
   */
  void ResetTaskFunc(std::function<void()> new_task_func);

  /**
   * @brief 将this协程切换到运行状态
   * @details 由线程中正在执行的其他协程调用，将this协程上下文和线程上下文进行切换
   */
  void Resume();

  /**
   * @brief 将this协程切换到挂起状态
   * @details 由this协程执行时主动调用，将this协程上下文和线程中存储的待恢复上下文进行切换
   */
  void Yield();

  /**
   * @brief 获取协程的id
   * @return 协程的id
   */
  auto GetId() const -> uint64_t;

  /**
   * @brief 获取协程的状态
   * @return 协程的状态
   */
  auto GetState() const -> State;

  /**
   * @brief 设置父协程
   */
  void SetParentCoroutine(std::weak_ptr<Coroutine> parent);

  /**
   * @brief 将当前线程设置为协程模式
   */
  static void InitThreadToCoMod();

  /**
   * @brief 修改线程局部变量，表示线程中正在执行的协程为curr
   * @param curr 将curr设置为当前正在执行的协程
   */
  static void SetThreadRunningCoroutine(Coroutine::s_ptr curr);

  /**
   * @brief 获取当前线程正在执行的协程
   */
  static auto GetThreadRunningCoroutine() -> s_ptr;

  /**
   * @brief 获取当前线程的主协程
   *
   * @return s_ptr
   */
  static auto GetThreadMainCoroutine() -> s_ptr;

  /**
   * @brief 获取所有协程的数量
   */
  static auto GetSystemCoroutineCount() -> uint64_t;

  /**
   * @brief 所有协程的主流程，task会在其中被执行
   * @details // TODO (wtsclwq) 为什么要设置为static？
   */
  static void MainFunc();

 private:
  /**
   * @brief 将无参构造函数私有化，不允许手动调用，因为创建协程必须传入具体的任务
   * @details
   * 为什么不delete？因为我们需要用该构造函数创建线程的第一个协程，也就是线程主函数对应的协程，
   * 此时不需要传入具体任务，因为任务就是线程主题，只需要初始化成员变量，并且将current_coroutine_指向this即可
   * 表明调用该构造函数的线程进入协程模式
   */
  Coroutine();
  uint64_t id_{0};                            // 协程的id
  uint32_t stack_size_{0};                    // 协程栈大小
  ucontext_t context_{};                      // 协程上下文
  State state_{State::Ready};                 // 协程状态
  void *stack_{nullptr};                      // 协程栈
  std::function<void()> task_func_{nullptr};  // 协程要执行的具体任务
  std::weak_ptr<Coroutine> parent_;           // 父协程
  bool has_parent_{false};                    // 是否有父协程
};
}  // namespace wtsclwq

#endif  // _WTSCLWQ_COROUTINE_