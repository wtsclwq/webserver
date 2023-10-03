#ifndef _WTSCLWQ_SCHEDULER_
#define _WTSCLWQ_SCHEDULER_

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <utility>
#include "coroutine.h"
#include "log.h"
#include "thread.h"

namespace wtsclwq {
class Scheduler : public std::enable_shared_from_this<Scheduler> {
 public:
  using s_ptr = std::shared_ptr<Scheduler>;
  using MutexType = std::mutex;

  explicit Scheduler(size_t thread_num = 1, bool use_creator = true, std::string_view name = "Scheduler");

  virtual ~Scheduler();

  auto GetName() -> std::string;

  /**
   * @brief 获取当前线程中的调度器指针
   */
  static auto GetThreadScheduler() -> s_ptr;

  /**
   * @brief 获取当前线程的调度协程
   * @details 对于线程池中的线程来说，调度协程==主协程， 对于creator线程来说，调度协程 != 主协程
   */
  static auto GetThreadScheduleCoroutine() -> Coroutine::s_ptr;

  /**
   * @brief 将协程对象或者函数对象加入到任务队列中
   * @details 在.cpp文件中实现，在.h文件中显示实例化Coroutine和函数对象版本
   */
  template <typename Scheduleable>
  void Schedule(Scheduleable sa, int target_thread_id = -1);

  /**
   * @brief 启动调度器
   */
  void Start();

  /**
   * @brief 停止调度器，需要等待所有任务执行完毕
   */
  void Stop();

 protected:
  /**
   * @brief 唤醒线程池中的线程，使其从任务队列中取出任务执行
   */
  virtual void Tickle();

  /**
   * @brief 调度器主逻辑，线程池中的线程会在这里取得任务并执行
   */
  void Run();

  /**
   * @brief 空闲线程的主逻辑，空闲线程会在这里等待任务
   */
  virtual void Idle();

  /**
   * @brief 返回是否可以停止调度器
   */
  virtual auto IsStopable() -> bool;

  /**
   * @brief 设置当前线程的调度器
   */
  void InitThreadScheduler();

  /**
   * @brief 是否有空闲线程
   * @details 当线程从Run进入Idle时，空闲线程数+1，反之-1
   */
  auto HasIdleThread() -> bool;

 private:
  /**
   * @brief 将协程对象加入到任务队列中，具体逻辑，无锁实现
   */
  template <typename Scheduleable>
  auto NonLockScheduleImpl(Scheduleable sa, int target_thread_id) -> bool;

  struct ScheduleTask {
    Coroutine::s_ptr coroutine_;
    std::function<void()> func_;
    int target_thread_id_;
    ScheduleTask(Coroutine::s_ptr coroutine, int thread_id) {
      coroutine_ = std::move(coroutine);
      target_thread_id_ = thread_id;
    }
    ScheduleTask(std::function<void()> func, int thread_id) {
      func_ = std::move(func);
      target_thread_id_ = thread_id;
    }
    ScheduleTask() {
      coroutine_ = nullptr;
      func_ = nullptr;
      target_thread_id_ = -1;
    }
    void Clear() {
      coroutine_ = nullptr;
      func_ = nullptr;
      target_thread_id_ = -1;
    }
    auto Empty() -> bool { return coroutine_ == nullptr && func_ == nullptr; }
  };
  std::string name_{};                                    // 调度器名称
  std::vector<Thread::s_ptr> thread_pool_{};              // 线程池
  std::vector<int> thread_ids_{};                         // 线程池中线程的id
  size_t thread_count_{0};                                // 线程池中线程的数量
  std::list<ScheduleTask> task_queue_{};                  // 任务队列
  std::atomic<size_t> active_thread_count_{0};            // 活跃线程数量
  std::atomic<size_t> idle_thread_count_{0};              // 空闲线程数量
  bool use_creator_thread_{false};                        // 是否使用创建者线程参与任务调度
  Coroutine::s_ptr creator_schedule_coroutine_{nullptr};  // 创建者线程的调度协程
  int creator_thread_id_{-1};                             // 创建者线程的id
  bool is_stoped_{false};                                 // 是否已经停止
  MutexType mutex_{};                                     // 互斥锁
};

}  // namespace wtsclwq

#endif  // _WTSCLWQ_SCHEDULER_