#ifndef _SOCK_IO_SCHEDULER_H_
#define _SOCK_IO_SCHEDULER_H_

#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include "coroutine.h"
#include "lock.h"
#include "scheduler.h"

namespace wtsclwq {
class SockIoScheduler : public Scheduler {
 public:
  using s_ptr = std::shared_ptr<SockIoScheduler>;
  using MutexType = std::shared_mutex;

  enum EventType {
    None = 0x1,   // 无事件
    Read = 0x1,   // 读时间（EPOLILIN）
    Write = 0x4,  // 写事件（EPOLLOUT）
  };

  /**
   * @brief 构造函数
   * @param thread_num 线程数量
   * @param use_creator 是否使用调用构造函数的创建者线程参与调度
   * @param name  调度器名称
   */
  explicit SockIoScheduler(size_t thread_num = 1, bool use_creator = true, std::string_view name = "SockIoScheduler");

  /**
   * @brief 析构函数，这里要override
   */
  ~SockIoScheduler() override;

  /**
   * @brief 向调度器添加一个fd上的某一个事件的监听任务
   * @param fd 目标fd
   * @param event_type 目标事件类型
   * @param func 事件回调函数，如果为空则默认将注册时线程的上下文封装成协程，表示事件发生时继续执行注册时的逻辑
   * @return bool 是否注册成功
   */
  auto AddEventListening(int fd, EventType event_type, std::function<void()> func = nullptr) -> bool;

  /**
   * @brief 从调度器中移除一个fd上的某一个事件的监听任务
   * @param fd 目标fd
   * @param enent_type 目标事件类型
   * @return bool 是否移除成功
   */
  auto RemoveEventListening(int fd, EventType enent_type) -> bool;

  /**
   * @brief 移除并触发一个fd上的某一个事件的监听任务
   * @param fd 目标fd
   * @param event_type 目标事件类型
   * @return bool 是否移除并触发成功
   */
  auto RemoveAndTriggerEventListening(int fd, EventType event_type) -> bool;

  /**
   * @brief 移除并触发一个fd上的所有事件的监听任务
   * @param fd 目标fd
   * @return 是否移除并触发成功
   */
  auto RemoveAllEventListening(int fd) -> bool;

  /**
   * @brief 唤醒线程池中的空闲线程
   */
  void Tickle() override;

  /**
   * @brief 判断当前调度器的状态是否可以停止
   */
  auto IsStopable() -> bool override;

  /**
   * @brief 线程空闲等待的主逻辑，线程池中的空闲线程会在这里等待任务队列就绪
   */
  void Idle() override;

  /**
   * @brief
   *
   */
  // void OnNewTimerAtFront() override;

  /**
   * @brief 判断当前调度器的状态是否可以停止，并且返回最近一个定时器任务的超时时间
   * @param[out] timeout 最近一个定时器的超时时间，用于idle协程的epoll_wait
   * @return 返回是否可以停止
   */
  auto IsStopableWithTime(uint64_t *timeout) -> bool;

  /**
   * @brief 扩容fd容器
   * @details 调度器管理的fd越来越多，自然需要扩容
   */
  void ContextVecResize(size_t size);

 private:
  struct FileDescContext {
    using s_ptr = std::shared_ptr<FileDescContext>;
    using MutexType = SpinLock;

    /**
     * @brief 事件上下文，用来存储io事件回调以及执行回调的调度器
     */
    struct EventContext {
      using s_ptr = std::shared_ptr<EventContext>;
      std::weak_ptr<SockIoScheduler> scheduler_{};  // 事件回调的调度器
      Coroutine::s_ptr coroutine_{nullptr};         // 事件回调协程
      std::function<void()> func_;                  // 事件回调函数
    };

    /**
     * @brief 获取某一个类型的事件上下文
     * @param event_type 读或写
     */
    auto GetEventContext(EventType event_type) -> EventContext::s_ptr;

    /**
     * @brief 重置某一个事件上下文
     */
    void ResetEventContext(EventContext::s_ptr event_context);

    /**
     * @brief 触发某一个事件，执行其回调
     * @param event_type 读或写
     */
    void TriggerEvent(EventType event_type);

    int sys_fd_{0};                          // 系统fd
    EventContext::s_ptr read_event_ctx_{};   // fd上的读事件上下文
    EventContext::s_ptr write_event_ctx_{};  // fd上的写事件上下文
    EventType event_type_{EventType::None};  // 该fd注册了那些事件类型，注意event_type可以通过位运算组合
    MutexType mutex_{};                      // 互斥锁，这里选择SpinLock因为锁的粒度很小
  };

  int epoll_fd_{0};
  int tickle_pipe_fds_[2]{};
  std::atomic<size_t> pending_event_count_{0};
  std::vector<FileDescContext::s_ptr> fd_contexts_{};
  std::shared_mutex mutex_{};
};
}  // namespace wtsclwq

#endif  // _SOCK_IO_SCHEDULER_H_