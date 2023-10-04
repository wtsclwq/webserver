#ifndef _TIMER_H_
#define _TIMER_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <shared_mutex>
namespace wtsclwq {
class TimerManager;
class Timer : public std::enable_shared_from_this<Timer> {
  friend class TimerManager;

 public:
  using s_ptr = std::shared_ptr<Timer>;

  /**
   * @brief 取消定时器任务
   */
  auto Cancel() -> bool;

  /**
   * @brief 刷新定时器
   * @details 重新设置定时器任务的执行时间为[当前时间+间隔时间]
   */
  auto Refresh() -> bool;

  /**
   * @brief 重新设置定时器任务的间隔时间以及是否从当前时间开始计算
   * @param new_interval_time 间隔时间
   * @param from_now 是否从当前时间开始计算
   */
  auto Reset(uint64_t new_interval_time, bool from_now) -> bool;

 private:
  Timer(uint64_t interval_time, bool recurring, std::function<void()> func, std::weak_ptr<TimerManager> manager);
  explicit Timer(uint64_t next);

  uint64_t interval_time_{0};              // 间隔时间
  uint64_t next_time_{0};                  // 下次执行时间
  bool recurring_{false};                  // 是否循环
  std::function<void()> func_{};           // 回调函数
  std::weak_ptr<TimerManager> manager_{};  // 所属定时器管理器
};

class TimerManager {
  friend class Timer;

 public:
  using s_ptr = std::shared_ptr<TimerManager>;
  using MutexType = std::shared_mutex;

  
private:
  // 定时器队列
  std::set<Timer::s_ptr, std::function<bool(Timer::s_ptr, Timer::s_ptr)>> timer_quque_{};
  // 是否需要唤醒空闲线程
  bool need_to_tickle_{false};
  // 上次执行时间
  uint64_t previouse_trigger_time_{0};
  // 互斥锁
  MutexType mutex_{};
};
}  // namespace wtsclwq

#endif  // _TIMER_H_