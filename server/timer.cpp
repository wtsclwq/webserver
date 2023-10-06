#include "timer.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <regex>
#include <shared_mutex>
#include <utility>
#include <vector>
#include "macro.h"
#include "utils.h"

namespace wtsclwq {
Timer::Timer(uint64_t interval_time, bool recurring, std::function<void()> func, std::weak_ptr<TimerManager> manager)
    : interval_time_(interval_time),
      next_time_(GetElapsedTime() + interval_time),
      recurring_(recurring),
      func_(std::move(func)),
      manager_(std::move(manager)) {}

Timer::Timer(uint64_t next_time) : next_time_(next_time) {}

auto Timer::Cancel() -> bool {
  auto manager_ptr = manager_.lock();
  ASSERT(manager_ptr != nullptr);
  std::lock_guard<TimerManager::MutexType> lock(manager_ptr->mutex_);
  if (func_ != nullptr) {
    func_ = nullptr;
    auto it = manager_ptr->timer_quque_.find(shared_from_this());
    manager_ptr->timer_quque_.erase(it);
    return true;
  }
  return false;
}

auto Timer::Refresh() -> bool {
  auto manager_ptr = manager_.lock();
  ASSERT(manager_ptr != nullptr);
  std::lock_guard<TimerManager::MutexType> lock(manager_ptr->mutex_);
  if (func_ == nullptr) {
    return false;
  }
  auto it = manager_ptr->timer_quque_.find(shared_from_this());
  if (it == manager_ptr->timer_quque_.end()) {
    return false;
  }
  manager_ptr->timer_quque_.erase(it);
  next_time_ = GetElapsedTime() + interval_time_;
  manager_ptr->timer_quque_.insert(shared_from_this());
  return true;
}

auto Timer::Reset(uint64_t new_interval_time, bool from_now) -> bool {
  // 如果没有变化，那就默认重置成功
  if (new_interval_time == interval_time_ && !from_now) {
    return true;
  }
  auto manager_ptr = manager_.lock();
  std::lock_guard<TimerManager::MutexType> lock(manager_ptr->mutex_);

  // 如果func_为空，说明该定时器可能已经被其他线程触发或者取消
  if (func_ == nullptr) {
    return false;
  }

  // 如果查找不到，说明已经被其他线程触发或者取消
  auto it = manager_ptr->timer_quque_.find(shared_from_this());
  if (it == manager_ptr->timer_quque_.end()) {
    return false;
  }

  // 从定时器队列中删除
  manager_ptr->timer_quque_.erase(it);

  // 重新设置定时器的间隔时间以及下次执行时间
  uint64_t start_time = 0;
  if (from_now) {
    start_time = GetElapsedTime();
  } else {
    start_time = next_time_ - interval_time_;
  }
  interval_time_ = new_interval_time;
  next_time_ = start_time + new_interval_time;

  // 重新插入定时器队列
  auto ret_it = manager_ptr->timer_quque_.insert(shared_from_this()).first;
  manager_ptr->has_new_front_timer_ = (ret_it == manager_ptr->timer_quque_.begin());
  return true;
}

TimerManager::TimerManager() {
  // 初始化定时器队列
  std::function<bool(Timer::s_ptr, Timer::s_ptr)> cmp = [](const Timer::s_ptr &lhs, const Timer::s_ptr &rhs) -> bool {
    if (lhs == nullptr && rhs == nullptr) {
      return false;
    }
    if (lhs == nullptr) {
      return true;
    }
    if (rhs == nullptr) {
      return false;
    }
    if (lhs->next_time_ < rhs->next_time_) {
      return true;
    }
    if (lhs->next_time_ > rhs->next_time_) {
      return false;
    }
    return lhs.get() < rhs.get();
  };
  timer_quque_ = std::set<Timer::s_ptr, decltype(cmp)>(cmp);
  previouse_trigger_time_ = GetElapsedTime();
}

TimerManager::~TimerManager() = default;

auto TimerManager::AddTimer(uint64_t interval_time, std::function<void()> func, bool recurring) -> Timer::s_ptr {
  std::lock_guard<MutexType> lock(mutex_);
  Timer::s_ptr new_timer(new Timer(interval_time, recurring, std::move(func), shared_from_this()));
  auto ret_it = timer_quque_.insert(new_timer).first;

  has_new_front_timer_ = (ret_it == timer_quque_.begin());
  return new_timer;
}

static void ConditionTimerFuncWrap(const std::function<bool()> &cond, const std::function<void()> &func) {
  // 如果条件不满足，那么就不执行回调
  if (cond()) {
    func();
  }
}

auto TimerManager::AddConditionTimer(uint64_t interval_time, const std::function<void()> &func,
                                     const std::function<bool()> &cond, bool recurring) -> Timer::s_ptr {
  return AddTimer(
      interval_time, [cond, func] { return ConditionTimerFuncWrap(cond, func); }, recurring);
}

auto TimerManager::GetRecentTriggerTime() -> uint64_t {
  std::shared_lock<MutexType> lock(mutex_);

  if (timer_quque_.empty()) {
    return UINT64_MAX;
  }

  auto recent_timer = *timer_quque_.begin();
  uint64_t curr_time = GetElapsedTime();
  // 如果当前时间已经超过了最近一个定时器的执行时间，那么就返回0，表示需要立即执行
  if (curr_time >= recent_timer->next_time_) {
    return 0;
  }
  // 否则返回一个时间间隔，从而让线程可以休眠一段时间，避免空转
  return recent_timer->next_time_ - curr_time;
}

auto TimerManager::GetAllTriggeringTimerFuncs() -> std::vector<std::function<void()>> {
  uint64_t curr_time = GetElapsedTime();
  std::vector<Timer::s_ptr> expired_timers;
  {
    std::shared_lock<MutexType> lock(mutex_);
    if (timer_quque_.empty()) {
      return {};
    }
  }

  std::lock_guard<MutexType> lock(mutex_);
  if (timer_quque_.empty()) {
    return {};
  }

  // bool rollover = DetectSysClockRollover(); 由于使用了CLOCK_MONOTONIC_RAW，应该不会出现时间回退的问题

  // 如果队列头部的触发时间大于当前时间，表明没有定时器需要触发
  if (auto it = timer_quque_.begin(); (*it)->next_time_ > curr_time) {
    return {};
  }

  // 触发时间为当前时间的定时器
  Timer::s_ptr curr_timer(new Timer(curr_time));
  // 利用当前时间找到所有需要触发的定时器，但是由于这里查找的是lower_bound，需要继续向后查找，避免错过触发时间相同的定时器
  auto target_it = timer_quque_.lower_bound(curr_timer);
  while (target_it != timer_quque_.end() && (*target_it)->next_time_ == curr_time) {
    ++target_it;
  }

  expired_timers.insert(expired_timers.end(), timer_quque_.begin(), target_it);
  timer_quque_.erase(timer_quque_.begin(), target_it);

  std::vector<std::function<void()>> res{};
  res.reserve(expired_timers.size());
  for (auto &t : expired_timers) {
    res.push_back(t->func_);
    // 如果是循环定时器，那么就需要重新插入队列
    if (t->recurring_) {
      t->next_time_ = curr_time + t->interval_time_;
      timer_quque_.insert(t);
    } else {
      t->func_ = nullptr;
    }
  }
  // 重置has_new_front_timer_标志位，下次再有新的定时器插入队列头部时，才会唤醒空闲线程
  has_new_front_timer_ = false;

  return res;
}

auto TimerManager::Empty() -> bool {
  std::shared_lock<MutexType> lock(mutex_);
  return timer_quque_.empty();
}

auto TimerManager::NeedTickle() -> bool { return has_new_front_timer_ && !recently_tickled_; }

void TimerManager::SetTickled() { recently_tickled_ = true; }

}  // namespace wtsclwq