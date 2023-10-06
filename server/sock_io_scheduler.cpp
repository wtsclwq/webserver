#include "sock_io_scheduler.h"
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <utility>
#include "log.h"
#include "macro.h"
#include "server/coroutine.h"
#include "server/fd_context.h"
#include "server/timer.h"

namespace wtsclwq {
static auto sys_logger = NAMED_LOGGER("system");

SockIoScheduler::SockIoScheduler(size_t thread_num, bool use_creator, std::string_view name)
    : Scheduler(thread_num, use_creator, name) {
  // 初始化epoll, 新版本的epoll不需要传入size，取而代之的是flag标志位， 目前仅支持EPOLL_CLOEXEC表示在exec时关闭epoll_fd
  epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
  ASSERT(epoll_fd_ > 0);

  // 初始化管道
  int ret = pipe(tickle_pipe_fds_);
  ASSERT(ret == 0);

  timer_manager_ = std::make_shared<TimerManager>();
}

SockIoScheduler::~SockIoScheduler() {
  close(epoll_fd_);
  close(tickle_pipe_fds_[0]);
  close(tickle_pipe_fds_[1]);
}

void SockIoScheduler::Start() {
  // 启动定时器
  // pipe有两端，我们让epoll监听读端的可读事件，这样当我们往写端写入数据时，epoll_wait就会返回，从而唤醒阻塞在epoll_wait的线程
  epoll_event event_info{};
  event_info.data.fd = tickle_pipe_fds_[0];  // 监听管道读端fd
  event_info.events = EPOLLIN | EPOLLET;     // 读事件、边缘触发

  // 第二个参数是此次操作的类型，F_SETFL表示设置文件状态标志，第三个参数是要设置的标志，O_NONBLOCK表示非阻塞
  int ret = fcntl(tickle_pipe_fds_[0], F_SETFL, O_NONBLOCK);  // 设置为非阻塞
  ASSERT(ret == 0);

  ret = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, tickle_pipe_fds_[0], &event_info);
  ASSERT(ret == 0);

  ContextVecResize(32);

  Scheduler::Start();
}

void SockIoScheduler::ContextVecResize(size_t size) {
  fd_contexts_.resize(size);
  for (size_t i = 0; i < fd_contexts_.size(); ++i) {
    if (fd_contexts_[i] == nullptr) {
      fd_contexts_[i] = std::make_shared<FileDescContext>();
      fd_contexts_[i]->sys_fd_ = i;
    }
  }
}

auto SockIoScheduler::AddEventListening(int target_fd, FileDescContext::EventType target_event_type,
                                        std::function<void()> cb_func) -> bool {
  FileDescContext::s_ptr fd_ctx = nullptr;
  {
    mutex_.lock_shared();  // 读锁
    if (fd_contexts_.size() > static_cast<size_t>(target_fd)) {
      // 如果fd_contexts_中已经存在了target_fd的fd_context，那么直接取出来
      fd_ctx = fd_contexts_[target_fd];
      mutex_.unlock_shared();  // 释放读锁
    } else {
      mutex_.unlock_shared();                   // 释放读锁
      std::lock_guard<MutexType> lock(mutex_);  // 范围写锁
      ContextVecResize(target_fd * 1.5);
      fd_ctx = fd_contexts_[target_fd];
    }
  }

  std::lock_guard<FileDescContext::MutexType> lock(fd_ctx->mutex_);
  // 不允许在上一次注册的事件还未触发时，重复注册该事件
  if ((fd_ctx->registered_event_types_ & target_event_type) != 0) {
    LOG_ERROR(sys_logger) << "fd: " << target_fd << " has already registered event: " << target_event_type;
    ASSERT(false)
  }
  // 如果该fd从未注册过事件，那么操作类型就是ADD，否则就是MOD
  int op = fd_ctx->registered_event_types_ == FileDescContext::EventType::None ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;

  // 先更新FDContext的状态，因为如果先向epoll注册了时间，有一种极限状态是事件触发了，但是我们的FDContext还没有更新，这样就会导致事件回调函数找不到
  //
  // 更新fd_context的状态
  fd_ctx->registered_event_types_ =
      static_cast<FileDescContext::EventType>(fd_ctx->registered_event_types_ | target_event_type);
  FileDescContext::EventContext::s_ptr event_ctx = fd_ctx->GetEventContext(target_event_type);
  // 可以确保：每个事件的上下文，在每次触发之后都会重置，因此如果该事件没有注册过，或者注册过但是已经触发过了，那么此时得到的event_ctx是空的
  ASSERT(event_ctx->scheduler_.lock() == nullptr && event_ctx->coroutine_ == nullptr && event_ctx->func_ == nullptr);
  event_ctx->scheduler_ = GetThreadScheduler();
  // 如果传入的cb_func为空，那么就将当前执行上下文封装成协程，作为回调
  if (cb_func == nullptr) {
    Coroutine::InitThreadToCoMod();
    event_ctx->coroutine_ = Coroutine::GetThreadRunningCoroutine();
    ASSERT(event_ctx->coroutine_->GetState() == Coroutine::State::Running);
  } else {
    event_ctx->func_ = std::move(cb_func);
  }

  epoll_event event_info{};
  // event_info.data.fd = target_fd;  // 似乎可有可无？
  event_info.events = EPOLLET | static_cast<uint32_t>(fd_ctx->registered_event_types_ | target_event_type);  // 边缘触发
  event_info.data.ptr = fd_ctx.get();  // 事件触发后，可以通过该指针获取到fd_context，从而获取到事件回调函数
  int ret = epoll_ctl(epoll_fd_, op, target_fd, &event_info);
  if (ret != 0) {
    LOG_ERROR(sys_logger) << "epoll_ctl failed, fd: " << target_fd << ", op: " << op
                          << ", event_type: " << target_event_type << ", errno: " << errno
                          << ", errstr: " << strerror(errno);
    return false;
  }

  // 待触发的事件数量+1
  ++pending_event_count_;

  return true;
}

auto SockIoScheduler::RemoveEventListening(int target_fd, FileDescContext::EventType target_event_type) -> bool {
  FileDescContext::s_ptr fd_ctx = nullptr;
  {
    std::shared_lock<MutexType> lock(mutex_);
    if (fd_contexts_.size() <= static_cast<size_t>(target_fd)) {
      return false;
    }
    fd_ctx = fd_contexts_[target_fd];
  }

  std::lock_guard<FileDescContext::MutexType> lock(fd_ctx->mutex_);
  // 如果目标事件没有注册过，那么直接返回
  if ((fd_ctx->registered_event_types_ & target_event_type) == 0) {
    return false;
  }

  // 清除epoll中目标fd上的目标事件
  auto new_event_types =
      static_cast<FileDescContext::EventType>(fd_ctx->registered_event_types_ & (~target_event_type));
  // 如果清除之后，fd上没有任何事件了，那么就从epoll中DEL该fd，否则是MOD
  int op = new_event_types == FileDescContext::EventType::None ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
  epoll_event event_info{};
  // event_info.data.fd = target_fd;
  event_info.events = EPOLLET | static_cast<uint32_t>(new_event_types);
  event_info.data.ptr = fd_ctx.get();
  int ret = epoll_ctl(epoll_fd_, op, target_fd, &event_info);
  if (ret != 0) {
    LOG_ERROR(sys_logger) << "epoll_ctl failed, fd: " << target_fd << ", op: " << op
                          << ", event_type: " << target_event_type << ", errno: " << errno
                          << ", errstr: " << strerror(errno);
    return false;
  }

  // 待触发的事件数量-1
  --pending_event_count_;

  // 更新fd_context的状态
  fd_ctx->registered_event_types_ = new_event_types;
  fd_ctx->ResetEventContext(target_event_type);
  return true;
}

auto SockIoScheduler::RemoveAndTriggerEventListening(int target_fd, FileDescContext::EventType target_event_type)
    -> bool {
  FileDescContext::s_ptr fd_ctx = nullptr;
  {
    std::shared_lock<MutexType> lock(mutex_);
    if (fd_contexts_.size() <= static_cast<size_t>(target_fd)) {
      return false;
    }
    fd_ctx = fd_contexts_[target_fd];
  }

  std::lock_guard<FileDescContext::MutexType> lock(fd_ctx->mutex_);
  // 如果目标事件没有注册过，那么直接返回
  if ((fd_ctx->registered_event_types_ & target_event_type) == 0) {
    return false;
  }

  auto new_event_types =
      static_cast<FileDescContext::EventType>(fd_ctx->registered_event_types_ & (~target_event_type));
  // 如果清除之后，fd上没有任何事件了，那么就从epoll中DEL该fd，否则是MOD
  int op = new_event_types == FileDescContext::EventType::None ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
  epoll_event event_info{};
  // event_info.data.fd = target_fd;
  event_info.events = EPOLLET | static_cast<uint32_t>(new_event_types);
  event_info.data.ptr = fd_ctx.get();
  int ret = epoll_ctl(epoll_fd_, op, target_fd, &event_info);
  if (ret != 0) {
    LOG_ERROR(sys_logger) << "epoll_ctl failed, fd: " << target_fd << ", op: " << op
                          << ", event_type: " << target_event_type << ", errno: " << errno
                          << ", errstr: " << strerror(errno);
    return false;
  }

  // 待触发的事件数量-1
  --pending_event_count_;
  // 触发事件
  fd_ctx->TriggerEvent(target_event_type);
  return true;
}

auto SockIoScheduler::RemoveAndTriggerAllTypeEventListening(int target_fd) -> bool {
  FileDescContext::s_ptr fd_ctx = nullptr;
  {
    std::shared_lock<MutexType> lock(mutex_);
    if (fd_contexts_.size() <= static_cast<size_t>(target_fd)) {
      return false;
    }
    fd_ctx = fd_contexts_[target_fd];
  }

  std::lock_guard<FileDescContext::MutexType> lock(fd_ctx->mutex_);
  // 如果目标事件没有注册过，那么直接返回
  if (fd_ctx->registered_event_types_ == FileDescContext::EventType::None) {
    return false;
  }

  // 清除epoll中目标fd上的所有事件
  int op = EPOLL_CTL_DEL;
  epoll_event event_info{};
  // event_info.data.fd = target_fd;
  event_info.events = EPOLLET;
  event_info.data.ptr = fd_ctx.get();
  int ret = epoll_ctl(epoll_fd_, op, target_fd, &event_info);
  if (ret != 0) {
    LOG_ERROR(sys_logger) << "epoll_ctl failed, fd: " << target_fd << ", op: " << op
                          << ", event_type: " << fd_ctx->registered_event_types_ << ", errno: " << errno
                          << ", errstr: " << strerror(errno);
    return false;
  }

  // 触发事件
  if ((fd_ctx->registered_event_types_ & FileDescContext::EventType::Read) != 0) {
    fd_ctx->TriggerEvent(FileDescContext::EventType::Read);
    --pending_event_count_;
  }
  if ((fd_ctx->registered_event_types_ & FileDescContext::EventType::Write) != 0) {
    fd_ctx->TriggerEvent(FileDescContext::EventType::Write);
    --pending_event_count_;
  }
  ASSERT(fd_ctx->registered_event_types_ == FileDescContext::EventType::None);

  return true;
}

auto SockIoScheduler::GetThreadSockIoScheduler() -> SockIoScheduler::s_ptr {
  return std::dynamic_pointer_cast<SockIoScheduler>(GetThreadScheduler());
}

void SockIoScheduler::Tickle() {
  LOG_DEBUG(sys_logger) << "SockIoScheduler::Tickle";
  if (!HasIdleThread()) {
    return;
  }

  // 往管道写端写入数据，唤醒阻塞在epoll_wait的线程
  uint8_t data = 1;
  ssize_t n = write(tickle_pipe_fds_[1], &data, sizeof(data));
  ASSERT(n == sizeof(data));
}

auto SockIoScheduler::IsStopable() -> bool {
  // 1. 没有需要触发的定时器
  // 2. 没有待触发的IO事件
  // 3. 线程池中的线程都是空闲的（Scheduler::IsStopable）
  return timer_manager_->GetRecentTriggerTime() == UINT64_MAX && pending_event_count_ == 0 && Scheduler::IsStopable();
}

void SockIoScheduler::Idle() {
  LOG_INFO(sys_logger) << "SockIoScheduler::Idle";
  const uint64_t max_events = 256;
  const int max_pipe_read_size = 256;
  // 为什么要用new，因为协程栈空间有限，如果用栈上的数组，那么可能会导致栈溢出
  // 为什么要用unique_ptr，因为shared_ptr默认使用delete，而不是delete[]，因此会导致内存泄漏
  // 但是c++14为unique_ptr新增了T[]的特化，因此可以用unique_ptr来管理数组
  std::unique_ptr<epoll_event[]> ready_events(new epoll_event[max_events]);
  std::vector<std::function<void()>> timer_cb_funcs;
  while (true) {
    if (IsStopable()) {
      LOG_DEBUG(sys_logger) << "SockIoScheduler::Idle, stopable exit";
      break;
    }
    uint64_t next_timeout = timer_manager_->GetRecentTriggerTime();
    int ret = 0;
    do {
      // 最大超时时间为5s，如果距离下一个定时器触发的时间大于5s，那么epoll_wait就等待5s
      const uint64_t max_timeout = 5000;
      next_timeout = std::min(next_timeout, max_timeout);
      ret = epoll_wait(epoll_fd_, ready_events.get(), max_events, next_timeout);
      if (ret == -1 && errno == EINTR) {
        // EINTR代表epoll_wait被信号中断，需要重新调用
        continue;
      }
      // epoll_wait成功（超时前有时间就绪或者超时），跳出循环等待
      break;
    } while (true);

    // 取出所有需要触发的定时器回调函数
    timer_cb_funcs = timer_manager_->GetAllTriggeringTimerFuncs();
    // 执行所有定时器回调函数
    for (const auto &timer_cb_func : timer_cb_funcs) {
      timer_cb_func();
    }
    timer_cb_funcs.clear();

    // 遍历所有就绪的事件，根据epoll_event中的data.ptr获取到fd_context，然后执行回调
    for (int i = 0; i < ret; i++) {
      epoll_event &event_info = ready_events[i];
      // 如果是用来唤醒空闲线程的管道，那么就丢弃管道中的数据，然后忽略这个事件
      if (event_info.data.fd == tickle_pipe_fds_[0]) {
        std::unique_ptr<uint8_t[]> dummy_data(new uint8_t[max_pipe_read_size]);
        while (read(tickle_pipe_fds_[0], dummy_data.get(), max_pipe_read_size) > 0) {
        }
        continue;
      }
      auto *fd_ctx = static_cast<FileDescContext *>(event_info.data.ptr);
      std::lock_guard<FileDescContext::MutexType> lock(fd_ctx->mutex_);
      // 如果fd上没有注册任何事件，那么直接忽略
      if (fd_ctx->registered_event_types_ == FileDescContext::EventType::None) {
        continue;
      }

      if ((event_info.events & (EPOLLERR | EPOLLHUP)) != 0) {
        // EPOLLERR：IO出错，比如写一个已经关闭的socket/pipe
        // EPOLLHUB：对端关闭连接
        // 此时
        // 一般来说，如果出现了，那么手动设置该fd上的读写事件都触发，然后尝试执行回调
        event_info.events |=
            static_cast<uint32_t>(FileDescContext::EventType::Read | FileDescContext::EventType::Write) &
            fd_ctx->registered_event_types_;
      }
      int real_events = FileDescContext::EventType::None;
      if ((event_info.events & EPOLLIN) != 0) {
        real_events |= FileDescContext::EventType::Read;
      }
      if ((event_info.events & EPOLLOUT) != 0) {
        real_events |= FileDescContext::EventType::Write;
      }
      if ((fd_ctx->registered_event_types_ & real_events) == 0) {
        // 如果fd上注册的事件和实际触发的事件不匹配，那么直接忽略
        continue;
      }

      // 剔除已经触发的事件，将未触发的事件重新注册到epoll中
      int left_events = fd_ctx->registered_event_types_ & (~real_events);
      int op = left_events == FileDescContext::EventType::None ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
      event_info.events = EPOLLET | static_cast<uint32_t>(left_events);
      int ret2 = epoll_ctl(epoll_fd_, op, fd_ctx->sys_fd_, &event_info);
      if (ret2 != 0) {
        LOG_ERROR(sys_logger) << "epoll_ctl failed, fd: " << fd_ctx->sys_fd_ << ", op: " << op
                              << ", event_type: " << fd_ctx->registered_event_types_ << ", errno: " << errno
                              << ", errstr: " << strerror(errno);
        continue;
      }

      // 触发事件回调
      if ((real_events & FileDescContext::EventType::Read) != 0) {
        fd_ctx->TriggerEvent(FileDescContext::EventType::Read);
        --pending_event_count_;
      }
      if ((real_events & FileDescContext::EventType::Write) != 0) {
        fd_ctx->TriggerEvent(FileDescContext::EventType::Write);
        --pending_event_count_;
      }
    }

    // 处理完了所有定时器和IO事件，那么就线程可以尝试去执行线程池任务队列中的任务了
    // 因为其实TriggerEvent只是将任务放入了线程池的任务队列中，而没有真正执行
    Coroutine::s_ptr curr = Coroutine::GetThreadRunningCoroutine();
    auto raw_ptr = curr.get();
    curr.reset();
    raw_ptr->Yield();
  }
}

auto SockIoScheduler::AddTimer(uint64_t interval_time, std::function<void()> func, bool recurring) -> Timer::s_ptr {
  auto res = timer_manager_->AddTimer(interval_time, std::move(func), recurring);
  if (timer_manager_->NeedTickle()) {
    Tickle();
    timer_manager_->SetTickled();
  }
  return res;
}

auto SockIoScheduler::AddConditionTimer(uint64_t interval_time, const std::function<void()> &func,
                                        const std::function<bool()> &cond, bool recurring) -> Timer::s_ptr {
  auto res = timer_manager_->AddConditionTimer(interval_time, func, cond, recurring);
  if (timer_manager_->NeedTickle()) {
    Tickle();
    timer_manager_->SetTickled();
  }
  return res;
}

}  // namespace wtsclwq