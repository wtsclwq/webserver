#include "lock.h"

namespace wtsclwq {

Semaphore::Semaphore(uint32_t value) { sem_init(&value_, 0, value); }

Semaphore::~Semaphore() { sem_destroy(&value_); }

void Semaphore::Post() { sem_post(&value_); }

void Semaphore::Wait() { sem_wait(&value_); }

SpinLock::SpinLock() { pthread_spin_init(&spinlock_, 0); }

SpinLock::~SpinLock() { pthread_spin_destroy(&spinlock_); }

void SpinLock::lock() { pthread_spin_lock(&spinlock_); }

void SpinLock::unlock() { pthread_spin_unlock(&spinlock_); }
}  // namespace wtsclwq