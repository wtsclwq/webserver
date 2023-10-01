#include "thread.h"
#include <pthread.h>
#include <functional>
#include "utils.h"

namespace wtsclwq {
static thread_local Thread *curr_thread = nullptr;
static thread_local std::string curr_thread_name = "Unknown";

auto Thread::GetCurrPtr() -> Thread * { return curr_thread; }

auto Thread::GetCurrName() -> std::string { return curr_thread_name; }

void Thread::SetCurrName(std::string_view name) { curr_thread_name = name; }

Thread::Thread(std::function<void()> task, std::string_view name) : task_(std::move(task)), name_(name) {
  if (name.empty()) {
    name_ = "Unknown";
  } else if (name.size() > 15) {
    name_ = name.substr(0, 15);
  }

  int ret = pthread_create(&sys_thread_, nullptr, &Thread::RealProcess, this);
  if (ret != 0) {
    throw std::runtime_error("pthread_create error");
  }
  sem_.Wait();
}

Thread::~Thread() {
  if (sys_thread_ != 0) {
    pthread_detach(sys_thread_);
  }
}

void Thread::Join() {
  if (sys_thread_ != 0) {
    int ret = pthread_join(sys_thread_, nullptr);
    if (ret != 0) {
      throw std::runtime_error("pthread_join error");
    }
    // 重置为0，表示线程已经结束
    sys_thread_ = 0;
  }
}

auto Thread::RealProcess(void *args) -> void * {
  // 构造函数中被传入pthread_create的this指针
  auto *thread = static_cast<Thread *>(args);

  // 设置Thread Local变量
  curr_thread = thread;
  curr_thread_name = thread->name_;

  // 设置this的成员变量
  thread->id_ = wtsclwq::GetCurrSysThreadId();
  pthread_setname_np(pthread_self(), thread->name_.c_str());

  // 将this->task_保存到局部变量中，然后清空this->task_，避免在执行task_时，由于this被析构，导致task_被析构，进而导致任务执行异常
  std::function<void()> task; // 用于保存this->task_
  thread->task_.swap(task);  // 交换this->task_和task，使得this->task_为空，task保存了this->task_的值

  thread->sem_.Post(); // 通知构造函数，线程已经启动成功
  task(); // 执行任务
  return nullptr;
}
}  // namespace wtsclwq