#include "hook.h"
#include <asm-generic/socket.h>
#include <bits/types/struct_timeval.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <cerrno>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include "fd_manager.h"
#include "server/config.h"
#include "server/coroutine.h"
#include "server/fd_context.h"
#include "server/log.h"
#include "server/sock_io_scheduler.h"
#include "server/timer.h"

static auto sys_logger = NAMED_LOGGER("system");
namespace wtsclwq {
static auto tcp_connect_timeout =
    ConfigMgr::GetInstance() -> GetOrAddDefaultConfigItem("tcp.connect.timeout", 5000, "tcp connect timeout");

static thread_local bool is_hook_enabled = false;

#define WRAP(FUNC) \
  FUNC(sleep)      \
  FUNC(usleep)     \
  FUNC(nanosleep)  \
  FUNC(socket)     \
  FUNC(connect)    \
  FUNC(accept)     \
  FUNC(read)       \
  FUNC(readv)      \
  FUNC(recv)       \
  FUNC(recvfrom)   \
  FUNC(recvmsg)    \
  FUNC(write)      \
  FUNC(writev)     \
  FUNC(send)       \
  FUNC(sendto)     \
  FUNC(sendmsg)    \
  FUNC(close)      \
  FUNC(fcntl)      \
  FUNC(ioctl)      \
  FUNC(setsockopt) \
  FUNC(getsockopt)

void HookInit() {
  static bool is_inited = false;
  if (is_inited) {
    return;
  }
#define FUNC(name) name##_f = reinterpret_cast<name##_func>(dlsym(RTLD_NEXT, #name));
  WRAP(FUNC)  // NOLINT
#undef FUNC
}

static uint64_t connect_timeout = -1;
struct __HookIniter {  // NOLINT
  __HookIniter() {
    HookInit();
    connect_timeout = tcp_connect_timeout->GetValue();
    tcp_connect_timeout->AddListener([](const auto &old_value, const auto &new_value) {
      LOG_INFO(sys_logger) << "tcp connect timeout changed from " << old_value << " to " << new_value;
      connect_timeout = new_value;
    });
  }
};

static __HookIniter __hook_initer{};  // NOLINT

auto IsHookEnabled() -> bool { return is_hook_enabled; }

void SetHookEnabled(bool v) { is_hook_enabled = v; }

}  // namespace wtsclwq

template <typename OriginFunc, typename... Args>
static auto DoIo(int fd, OriginFunc origin_func, std::string_view hook_fun_name, uint32_t event_type, int timeout_type,
                 Args &&...args) -> ssize_t {
  if (!wtsclwq::IsHookEnabled()) {
    return origin_func(fd, std::forward<Args>(args)...);
  }

  auto fd_info_wrapper = wtsclwq::FdWrapperMgr::GetInstance()->Get(fd);
  // 如果fd没有被包装,或者说传入了一个错误的fd，那么直接调用原始函数
  if (fd_info_wrapper == nullptr) {
    return origin_func(fd, std::forward<Args>(args)...);
  }

  // 如果fd被关闭了，返回错误并设置errno
  if (fd_info_wrapper->IsClosed()) {
    errno = EBADF;
    return -1;
  }
  // 如果fd不是sockfd或者用户主动设置了fd为非阻塞模式，那么我们直接调用原始函数，让行为符合用户的预期
  if (!fd_info_wrapper->IsSocket() || fd_info_wrapper->IsUserLevelNonBlock()) {
    return origin_func(fd, std::forward<Args>(args)...);
  }

  uint64_t time_out = fd_info_wrapper->GetTimeout(timeout_type);
  // 条件定时器的条件，如果监听IO事件就绪之前，定时器触发，那么我们需要取消监听事件并返回错误
  // 反之，如果在定时器触发之前，IO事件就绪了，那么我们需要取消定时器
  auto timer_triggered = std::make_shared<bool>(false);

retry:
  ssize_t len = origin_func(fd, std::forward<Args>(args)...);
  // 如果是EINTR错误，那么我们需要重试
  while (len == -1 && errno == EINTR) {
    len = origin_func(fd, std::forward<Args>(args)...);
  }
  // 如果是EAGAIN错误，表示当前IO还没有就绪，需要利用SockIoScheduler来等待IO就绪并执行回调（这里的回调协程就是回到当前上下文继续执行）
  if (len == -1 && errno == EAGAIN) {
    auto sock_io_scheduler = wtsclwq::SockIoScheduler::GetThreadSockIoScheduler();
    wtsclwq::Timer::s_ptr timer = nullptr;
    std::weak_ptr<bool> timer_triggered_weak_ptr(timer_triggered);
    // 只有该函数返回true，定时器才能执行回调
    std::function<bool()> cond = [timer_triggered_weak_ptr]() -> bool {
      auto s_ptr = timer_triggered_weak_ptr.lock();
      // 条件非空，且没有改变
      return s_ptr != nullptr && !(*s_ptr);
    };
    // 为什么一定要用条件定时器？
    // 在多线程情况下，有可能出现IO事件还没就绪，所有定时器没有被Cancel，然后被线程A取走。
    // 在该线程逐个执行定时器的过程中，IO事件就绪，另一个线程B执行了IO事件的回调，即回到这里继续执行，但是它却无法Cancel掉线程A取走的定时器。
    // 如果不使用条件定时器，那线程A就能够顺利执行定时器的回调，这样就会导致线程B执行了IO事件的回调，线程A也执行了定时器的回调。
    // 如果使用一个weak_ptr作为条件，当线程B顺利完成IO事件的回调，结束了当前函数时，weak_ptr的宿主就会析构，那么线程A就不会执行定时器的回调了。
    std::function<void()> timer_cb = [timer_triggered_weak_ptr, &fd, &sock_io_scheduler, &event_type]() {
      auto s_ptr = timer_triggered_weak_ptr.lock();
      *s_ptr = true;
      sock_io_scheduler->RemoveAndTriggerEventListening(fd,
                                                        static_cast<wtsclwq::FileDescContext::EventType>(event_type));
    };

    if (time_out != UINT64_MAX) {
      timer = sock_io_scheduler->AddConditionTimer(time_out, timer_cb, cond);
    }
    bool add_sucess =
        sock_io_scheduler->AddEventListening(fd, static_cast<wtsclwq::FileDescContext::EventType>(event_type));
    if (!add_sucess) {
      LOG_ERROR(sys_logger) << "add event listening error, fd = " << fd << ", event_type = " << event_type;
      if (timer != nullptr) {
        timer->Cancel();
      }
      return -1;
    }
    // 为什么这里不需要获取raw_ptr？
    wtsclwq::Coroutine::GetThreadRunningCoroutine()->Yield();
    if (timer != nullptr) {
      timer->Cancel();
    }
    if (*timer_triggered) {
      errno = ETIMEDOUT;
      return -1;
    }
    goto retry;
  }
  return len;
}

extern "C" {
#define FUNC(name) name##_func name##_f = nullptr;
WRAP(FUNC);
#undef FUNC

auto sleep(unsigned int seconds) -> unsigned int {
  if (!wtsclwq::IsHookEnabled() || seconds == 0) {
    return sleep_f(seconds);
  }

  auto sock_io_scheduler = wtsclwq::SockIoScheduler::GetThreadSockIoScheduler();
  auto curr_coroutine = wtsclwq::Coroutine::GetThreadRunningCoroutine();
  std::function<void()> timer_cb = [sock_io_scheduler, curr_coroutine] { sock_io_scheduler->Schedule(curr_coroutine); };
  if (seconds != 0) {
    sock_io_scheduler->AddTimer(seconds * 1000, timer_cb);
  }
  curr_coroutine->Yield();
  return 0;
}

auto usleep(useconds_t useconds) -> int {
  if (!wtsclwq::IsHookEnabled() || useconds == 0) {
    return usleep_f(useconds);
  }
  auto sock_io_scheduler = wtsclwq::SockIoScheduler::GetThreadSockIoScheduler();
  auto curr_coroutine = wtsclwq::Coroutine::GetThreadRunningCoroutine();
  std::function<void()> timer_cb = [sock_io_scheduler, curr_coroutine] { sock_io_scheduler->Schedule(curr_coroutine); };
  if (useconds != 0) {
    sock_io_scheduler->AddTimer(useconds / 1000, timer_cb);
  }
  curr_coroutine->Yield();
  return 0;
}

auto nanosleep(const struct timespec *requested_time, struct timespec *remaining) -> int {
  if (!wtsclwq::IsHookEnabled()) {
    return nanosleep_f(requested_time, remaining);
  }
  auto sock_io_scheduler = wtsclwq::SockIoScheduler::GetThreadSockIoScheduler();
  auto curr_coroutine = wtsclwq::Coroutine::GetThreadRunningCoroutine();
  std::function<void()> timer_cb = [sock_io_scheduler, curr_coroutine] { sock_io_scheduler->Schedule(curr_coroutine); };
  if (requested_time != nullptr) {
    uint64_t timeout_ms = requested_time->tv_sec * 1000 + requested_time->tv_nsec / 1000 / 1000;
    sock_io_scheduler->AddTimer(timeout_ms, timer_cb);
  }
  curr_coroutine->Yield();
  return 0;
}

/**
 * @brief hook了socket申请，这样每次申请的socket都默认是非阻塞的
 */
auto socket(int domain, int type, int protocol) -> int {
  if (!wtsclwq::IsHookEnabled()) {
    return socket_f(domain, type, protocol);
  }
  int fd = socket_f(domain, type, protocol);
  if (fd == -1) {
    return fd;
  }
  wtsclwq::FdWrapperMgr::GetInstance()->Get(fd, true);
  return fd;
}

/**
 * @brief hook了connect，将connect赋予超时功能
 * @details connect和doio最大的差别就是connect在IO事件就绪之后，不需要再次调用connect，而是直接返回
 */
auto ConnectWithTimeout(int fd, const struct sockaddr *addr, socklen_t addrlen, uint64_t timeout_ms) -> int {
  if (!wtsclwq::IsHookEnabled()) {
    return connect_f(fd, addr, addrlen);
  }
  auto fd_info_wrapper = wtsclwq::FdWrapperMgr::GetInstance()->Get(fd);
  if (fd_info_wrapper == nullptr || fd_info_wrapper->IsClosed()) {
    errno = EBADF;
    return -1;
  }
  if (!fd_info_wrapper->IsSocket() || fd_info_wrapper->IsUserLevelNonBlock()) {
    return connect_f(fd, addr, addrlen);
  }
  if (timeout_ms == 0) {
    return connect_f(fd, addr, addrlen);
  }
  int ret = connect_f(fd, addr, addrlen);
  if (ret == 0) {
    return 0;
  }
  if (ret != -1 || errno != EINPROGRESS) {
    return ret;
  }

  auto sock_io_scheduler = wtsclwq::SockIoScheduler::GetThreadSockIoScheduler();
  wtsclwq::Timer::s_ptr timer = nullptr;
  std::shared_ptr<bool> timer_triggered = std::make_shared<bool>(false);
  std::weak_ptr<bool> timer_triggered_weak_ptr(timer_triggered);
  std::function<bool()> cond = [timer_triggered_weak_ptr]() -> bool {
    auto s_ptr = timer_triggered_weak_ptr.lock();
    return s_ptr != nullptr && !(*s_ptr);
  };
  std::function<void()> timer_cb = [sock_io_scheduler, fd, timer_triggered_weak_ptr]() {
    auto s_ptr = timer_triggered_weak_ptr.lock();
    *s_ptr = true;
    sock_io_scheduler->RemoveAndTriggerEventListening(fd, wtsclwq::FileDescContext::EventType::Write);
  };
  if (timeout_ms != UINT64_MAX) {
    timer = sock_io_scheduler->AddConditionTimer(timeout_ms, timer_cb, cond);
  }
  bool add_success = sock_io_scheduler->AddEventListening(fd, wtsclwq::FileDescContext::EventType::Write);
  if (add_success) {
    // 如果事件监听注册成功，那么我们需要让出当前协程的执行权，等待事件就绪后，回到当前上下文继续执行
    wtsclwq::Coroutine::GetThreadRunningCoroutine()->Yield();
    // 如果IO事件就绪了，那么我们需要取消定时器
    if (timer != nullptr) {
      timer->Cancel();
    }
    // 如果在处理IO之前，定时器触发了，那么我们需要返回错误
    if (*timer_triggered) {
      errno = ETIMEDOUT;
      return -1;
    }
  } else {
    // 如果事件监听注册失败，那么我们需要取消定时器
    if (timer != nullptr) {
      timer->Cancel();
    }
    LOG_ERROR(sys_logger) << "add event listening error, fd = " << fd;
  }

  int error = 0;
  socklen_t len = sizeof(error);
  ret = getsockopt_f(fd, SOL_SOCKET, SO_ERROR, &error, &len);
  if (ret == -1) {
    errno = EBADF;
    return -1;
  }
  if (error == 0) {
    return 0;
  }
  errno = error;
  return -1;
}

auto connect(int fd, const struct sockaddr *addr, socklen_t len) -> int {
  return ConnectWithTimeout(fd, addr, len, wtsclwq::connect_timeout);
}

auto accept(int fd, struct sockaddr *addr, socklen_t *len) -> int {
  return DoIo(fd, accept_f, "accept", wtsclwq::FileDescContext::EventType::Read, SO_RCVTIMEO, addr, len);
}

auto read(int fd, void *buf, size_t nbytes) -> ssize_t {
  return DoIo(fd, read_f, "read", wtsclwq::FileDescContext::EventType::Read, SO_RCVTIMEO, buf, nbytes);
}

auto readv(int fd, const struct iovec *iov, int iovcnt) -> ssize_t {  // NOLINT
  return DoIo(fd, readv_f, "readv", wtsclwq::FileDescContext::EventType::Read, SO_RCVTIMEO, iov, iovcnt);
}

auto recv(int fd, void *buf, size_t n, int flags) -> ssize_t {
  return DoIo(fd, recv_f, "recv", wtsclwq::FileDescContext::EventType::Read, SO_RCVTIMEO, buf, n, flags);
}

auto recvfrom(int fd, void *buf, size_t n, int flags, struct sockaddr *addr, socklen_t *addr_len) -> ssize_t {
  return DoIo(fd, recvfrom_f, "recvfrom", wtsclwq::FileDescContext::EventType::Read, SO_RCVTIMEO, buf, n, flags, addr,
              addr_len);
}

auto recvmsg(int fd, struct msghdr *message, int flags) -> ssize_t {
  return DoIo(fd, recvmsg_f, "recvmsg", wtsclwq::FileDescContext::EventType::Read, SO_RCVTIMEO, message, flags);
}

auto write(int fd, const void *buf, size_t n) -> ssize_t {
  return DoIo(fd, write_f, "write", wtsclwq::FileDescContext::EventType::Write, SO_SNDTIMEO, buf, n);
}

auto writev(int fd, const struct iovec *iov, int iovcnt) -> ssize_t {  // NOLINT
  return DoIo(fd, writev_f, "writev", wtsclwq::FileDescContext::EventType::Write, SO_SNDTIMEO, iov, iovcnt);
}

auto send(int fd, const void *buf, size_t n, int flags) -> ssize_t {
  return DoIo(fd, send_f, "send", wtsclwq::FileDescContext::EventType::Write, SO_SNDTIMEO, buf, n, flags);
}

auto sendto(int fd, const void *buf, size_t n, int flags, const struct sockaddr *addr, socklen_t addr_len) -> ssize_t {
  return DoIo(fd, sendto_f, "sendto", wtsclwq::FileDescContext::EventType::Write, SO_SNDTIMEO, buf, n, flags, addr,
              addr_len);
}

auto sendmsg(int fd, const struct msghdr *message, int flags) -> ssize_t {
  return DoIo(fd, sendmsg_f, "sendmsg", wtsclwq::FileDescContext::EventType::Write, SO_SNDTIMEO, message, flags);
}

auto close(int fd) -> int {
  if (!wtsclwq::IsHookEnabled()) {
    return close_f(fd);
  }
  auto fd_info_wrapper = wtsclwq::FdWrapperMgr::GetInstance()->Get(fd);
  if (fd_info_wrapper == nullptr) {
    return close_f(fd);
  }

  auto ret = close_f(fd);
  if (ret == 0) {
    auto sock_io_scheduler = wtsclwq::SockIoScheduler::GetThreadSockIoScheduler();
    if (sock_io_scheduler != nullptr) {
      sock_io_scheduler->RemoveAndTriggerAllTypeEventListening(fd);
    }
    wtsclwq::FdWrapperMgr::GetInstance()->Remove(fd);
  }
  return ret;
}

auto fcntl(int fd, int cmd, ...) -> int {
  va_list arg_list;
  va_start(arg_list, cmd);
  switch (cmd) {
    case F_SETFL: {
      int arg = va_arg(arg_list, int);
      va_end(arg_list);
      auto fd_info_wrapper = wtsclwq::FdWrapperMgr::GetInstance()->Get(fd);
      // 不归我们管
      if (fd_info_wrapper == nullptr || fd_info_wrapper->IsClosed() || !fd_info_wrapper->IsSocket()) {
        return fcntl_f(fd, cmd, arg);
      }
      fd_info_wrapper->SetUserLevelNonBlock((arg & O_NONBLOCK) != 0);
      // 因为我们有可能在socket阶段就设置了非阻塞，所以我们需要判断一下，避免在fcntl之后丢失设置
      if (fd_info_wrapper->IsSysLevelNonBlock()) {
        arg |= O_NONBLOCK;
      } else {
        arg &= ~O_NONBLOCK;
      }
      return fcntl_f(fd, cmd, arg);
    }
    case F_GETFL: {
      va_end(arg_list);
      int arg = fcntl_f(fd, cmd);
      auto fd_info_wrapper = wtsclwq::FdWrapperMgr::GetInstance()->Get(fd);
      if (fd_info_wrapper == nullptr || fd_info_wrapper->IsClosed() || !fd_info_wrapper->IsSocket()) {
        return arg;
      }
      if (fd_info_wrapper->IsUserLevelNonBlock()) {
        return arg | O_NONBLOCK;
      }
      return arg & ~O_NONBLOCK;
    }
    case F_DUPFD:
    case F_DUPFD_CLOEXEC:
    case F_SETFD:
    case F_SETOWN:
    case F_SETSIG:
    case F_SETLEASE:
    case F_NOTIFY:
#ifdef F_SETPIPE_SZ
    case F_SETPIPE_SZ: {
      int arg = va_arg(arg_list, int);
      va_end(arg_list);
      return fcntl_f(fd, cmd, arg);
    }
#endif
#ifdef F_GETPIPE_SZ
    case F_GETPIPE_SZ: {
      va_end(arg_list);
      return fcntl_f(fd, cmd);
    }
#endif
    case F_SETLK:
    case F_SETLKW:
    case F_GETLK: {
      struct flock *arg = va_arg(arg_list, struct flock *);
      va_end(arg_list);
      return fcntl_f(fd, cmd, arg);
    }
    case F_GETOWN_EX:
    case F_SETOWN_EX: {
      struct f_owner_exlock *arg = va_arg(arg_list, struct f_owner_exlock *);
      va_end(arg_list);
      return fcntl_f(fd, cmd, arg);
    }
    default:
      va_end(arg_list);
      return fcntl_f(fd, cmd);
  }
}

auto ioctl(int fd, uint64_t request, ...) -> int {
  va_list va;
  va_start(va, request);
  void *arg = va_arg(va, void *);
  va_end(va);
  if (FIONBIO == request) {
    bool user_nonblock = !(*static_cast<int *>(arg) == 0);
    auto fd_info_wrapper = wtsclwq::FdWrapperMgr::GetInstance()->Get(fd);
    if (fd_info_wrapper == nullptr || fd_info_wrapper->IsClosed() || !fd_info_wrapper->IsSocket()) {
      return ioctl_f(fd, request, arg);
    }
    fd_info_wrapper->SetUserLevelNonBlock(user_nonblock);
  }
  return ioctl_f(fd, request, arg);
}

auto getsockopt(int fd, int level, int optname, void *__restrict optval, socklen_t *__restrict optlen) -> int {
  return getsockopt_f(fd, level, optname, optval, optlen);
}

auto setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen) -> int {
  if (!wtsclwq::IsHookEnabled()) {
    return setsockopt_f(fd, level, optname, optval, optlen);
  }
  if (level == SOL_SOCKET) {
    if (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
      auto fd_info_wrapper = wtsclwq::FdWrapperMgr::GetInstance()->Get(fd);
      if (fd_info_wrapper != nullptr) {
        const auto *time_val = static_cast<const timeval *>(optval);
        uint64_t timeout_ms = time_val->tv_sec * 1000 + time_val->tv_usec / 1000;
        fd_info_wrapper->SetTimeout(optname, timeout_ms);
      }
    }
  }
  return setsockopt_f(fd, level, optname, optval, optlen);
}
}