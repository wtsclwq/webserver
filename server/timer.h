#ifndef _TIMER_H_
#define _TIMER_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <shared_mutex>
#include <vector>
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

class TimerManager : public std::enable_shared_from_this<TimerManager> {
  friend class Timer;

 public:
  using s_ptr = std::shared_ptr<TimerManager>;
  using MutexType = std::shared_mutex;

  TimerManager();

  virtual ~TimerManager();

  /**
   * @brief 添加定时器任务
   * @param interval_time 间隔时间
   * @param func 回调
   * @param recurring 是否循环
   */
  auto AddTimer(uint64_t interval_time, std::function<void()> func, bool recurring = false) -> Timer::s_ptr;

  auto AddConditionTimer(uint64_t interval_time, const std::function<void()> &func, const std::function<bool()> &cond,
                         bool recurring = false) -> Timer::s_ptr;
  /**
   * @brief 获取最近距离最近一个定时器触发的时间
   */
  auto GetRecentTriggerTime() -> uint64_t;

  /**
   * @brief 获取当前时间所有需要触发的定时器回调
   */
  auto GetAllTriggeringTimerFuncs() -> std::vector<std::function<void()>>;

  /**
   * @brief 定时器列表是否为空
   */
  auto Empty() -> bool;

 protected:
  /**
   * @brief 当有新的定时器，插入到队列头部（需要最早触发）时，唤醒空闲线程
   */
  virtual void OnNewTimerAtFront() = 0;

 private:

  // 定时器队列
  std::set<Timer::s_ptr, std::function<bool(Timer::s_ptr, Timer::s_ptr)>> timer_quque_{};
  // 是否需要唤醒空闲线程
  bool recently_tickled_{false};
  // 上次执行时间
  uint64_t previouse_trigger_time_{0};
  // 互斥锁
  MutexType mutex_{};
};
}  // namespace wtsclwq

#endif  // _TIMER_H_