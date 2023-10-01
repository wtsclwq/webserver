#ifndef _WTSCLWQ_LOCK_
#define _WTSCLWQ_LOCK_

#include <pthread.h>
#include <semaphore.h>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include "noncopyable.h"

namespace wtsclwq {
class Semaphore : Noncopyable {
 public:
  explicit Semaphore(uint32_t value = 0);
  ~Semaphore() override;
  void Wait();
  void Post();

 private:
  sem_t value_;
};

// RAII 读锁
using ReadLockGuard = std::shared_lock<std::shared_mutex>;
// RAII 写锁
using WriteLockGuard = std::lock_guard<std::shared_mutex>;

class SpinLock {
 public:
  SpinLock();
  ~SpinLock();
  void lock();    // NOLINT
  void unlock();  // NOLINT

 private:
  pthread_spinlock_t spinlock_;
};

}  // namespace wtsclwq

#endif