#include "timer.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <regex>
#include <utility>
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
  if (func_ != nullptr) {
    return false;
  }

  auto it = manager_ptr->timer_quque_.find(shared_from_this());
  if (it == manager_ptr->timer_quque_.end()) {
    return false;
  }
  manager_ptr->timer_quque_.erase(it);
  uint64_t start_time = 0;
  if (from_now) {
    start_time = GetElapsedTime();
  } else {
    start_time = next_time_ - interval_time_;
  }
  interval_time_ = new_interval_time;
  next_time_ = start_time + new_interval_time;
  manager_ptr->Add()
}
}  // namespace wtsclwq