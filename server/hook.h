#ifndef _WTSCLWQ_HOOK_
#define _WTSCLWQ_HOOK_

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdint>
#include <ctime>

namespace wtsclwq {
// 检查当前线程是否使用hook io
auto IsHookEnabled() -> bool;

// 启用当前线程的hook io
void EnableHoo();
}  // namespace wtsclwq

extern "C" {
// sleep系列
using sleep_func = unsigned int (*)(unsigned int);
extern sleep_func sleep_f;

using usleep_func = int (*)(useconds_t);
extern usleep_func usleep_f;

using nanosleep_func = int (*)(const struct timespec *, struct timespec *);
extern nanosleep_func nanosleep_f;

// socket系列
using socket_func = int (*)(int, int, int);
extern socket_func socket_f;

using connect_func = int (*)(int, const struct sockaddr *, socklen_t);
extern connect_func connect_f;

using accept_func = int (*)(int, struct sockaddr *, socklen_t *);
extern accept_func accept_f;

// read系列
using read_func = ssize_t (*)(int, void *, size_t);
extern read_func read_f;

using readv_func = ssize_t (*)(int, const struct iovec *, int);
extern readv_func readv_f;

using recv_func = ssize_t (*)(int, void *, size_t, int);
extern recv_func recv_f;

using recvfrom_func = ssize_t (*)(int, void *, size_t, int, struct sockaddr *, socklen_t *);
extern recvfrom_func recvfrom_f;

using recvmsg_func = ssize_t (*)(int, struct msghdr *, int);
extern recvmsg_func recvmsg_f;

// write系列
using write_func = ssize_t (*)(int, const void *, size_t);
extern write_func write_f;

using writev_func = ssize_t (*)(int, const struct iovec *, int);
extern writev_func writev_f;

using send_func = ssize_t (*)(int, const void *, size_t, int);
extern send_func send_f;

using sendto_func = ssize_t (*)(int, const void *, size_t, int, const struct sockaddr *, socklen_t);
extern sendto_func sendto_f;

using sendmsg_func = ssize_t (*)(int, const struct msghdr *, int);
extern sendmsg_func sendmsg_f;

// close系列
using close_func = int (*)(int);
extern close_func close_f;

// ioctl系列
using fcntl_func = int (*)(int, int, ...);
extern fcntl_func fcntl_f;

using ioctl_func = int (*)(int, uint64_t, ...);
extern ioctl_func ioctl_f;

using getsockopt_func = int (*)(int, int, int, void *, socklen_t *);
extern getsockopt_func getsockopt_f;

using setsockopt_func = int (*)(int, int, int, const void *, socklen_t);
extern setsockopt_func setsockopt_f;

// 其他
extern auto ConnectWithTimeout(int sockfd, const struct sockaddr *addr, socklen_t addrlen, uint64_t timeout) -> int;
}

#endif  // _WTSCLWQ_HOOK_