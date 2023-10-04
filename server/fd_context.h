#ifndef _WTSCLWQ_FD_CONTEXT_
#define _WTSCLWQ_FD_CONTEXT_

#include <memory>
#include "coroutine.h"
#include "lock.h"
#include "scheduler.h"

namespace wtsclwq {
struct FileDescContext {
  enum EventType {
    None = 0x1,   // 无事件
    Read = 0x1,   // 读时间（EPOLILIN）
    Write = 0x4,  // 写事件（EPOLLOUT）
  };

  using s_ptr = std::shared_ptr<FileDescContext>;
  using MutexType = SpinLock;

  /**
   * @brief 事件上下文，用来存储io事件回调以及执行回调的调度器
   */
  struct EventContext {
    using s_ptr = std::shared_ptr<EventContext>;
    std::weak_ptr<Scheduler> scheduler_{};  // 事件回调的调度器
    Coroutine::s_ptr coroutine_{nullptr};   // 事件回调协程
    std::function<void()> func_;            // 事件回调函数
  };

  /**
   * @brief 获取某一个类型的事件上下文
   * @param event_type 读或写
   */
  auto GetEventContext(EventType event_type) -> EventContext::s_ptr;

  /**
   * @brief 重置某一个事件上下文
   */
  void ResetEventContext(EventType event_type);

  /**
   * @brief 触发某一个事件，执行其回调
   * @param event_type 读或写
   */
  void TriggerEvent(EventType event_type);

  int sys_fd_{0};                          // 系统fd
  EventContext::s_ptr read_event_ctx_{};   // fd上的读事件上下文
  EventContext::s_ptr write_event_ctx_{};  // fd上的写事件上下文
  EventType registered_event_types_{EventType::None};  // 该fd注册了那些事件类型，注意event_type可以通过位运算组合
  MutexType mutex_{};                                  // 互斥锁，这里选择SpinLock因为锁的粒度很小
};

}  // namespace wtsclwq

#endif  // _WTSCLWQ_FD_CONTEXT_
