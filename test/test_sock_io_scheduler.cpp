#include <arpa/inet.h>
#include <asm-generic/errno.h>
#include <asm-generic/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <functional>
#include <memory>
#include "server/fd_context.h"
#include "server/log.h"
#include "server/scheduler.h"
#include "server/server.h"
#include "server/sock_io_scheduler.h"

auto root_logger = ROOT_LOGGER;

int sock_fd;
void WatchIoRead();

void DoIoWrite() {
  LOG_INFO(root_logger) << "DoIoWrite";
  int so_err;
  socklen_t len = sizeof(so_err);
  getsockopt(sock_fd, SOL_SOCKET, SO_ERROR, &so_err, &len);
  if (so_err != 0) {
    LOG_ERROR(root_logger) << "connect fail: " << so_err;
    return;
  }
  LOG_INFO(root_logger) << "connect success";
}

void DoIoRead() {
  LOG_INFO(root_logger) << "DoIoRead";
  char buf[1024];
  int n = read(sock_fd, buf, 1024);
  if (n > 0) {
    buf[n] = '\0';
    LOG_INFO(root_logger) << "read: " << buf << "len: " << n;
  } else if (n == 0) {
    LOG_INFO(root_logger) << "read EOF";
    close(sock_fd);
    return;
  } else {
    LOG_ERROR(root_logger) << "read error"
                           << "errno=" << errno << ", errstr = " << strerror(errno);
    close(sock_fd);
    return;
  }
  wtsclwq::SockIoScheduler::GetThreadSockIoScheduler()->Schedule(std::function<void()>(WatchIoRead));
}

void WatchIoRead() {
  LOG_INFO(root_logger) << "WatchIoRead";
  wtsclwq::SockIoScheduler::GetThreadSockIoScheduler()->AddEventListening(sock_fd, wtsclwq::FileDescContext::Read,
                                                                          DoIoRead);
}

void TestIo() {
  sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  ASSERT(sock_fd > 0);
  fcntl(sock_fd, F_SETFL, O_NONBLOCK);

  sockaddr_in serve_addr{};
  serve_addr.sin_family = AF_INET;
  serve_addr.sin_port = htons(9001);
  inet_pton(AF_INET, "127.0.0.1", &serve_addr.sin_addr.s_addr);

  int ret = connect(sock_fd, reinterpret_cast<sockaddr *>(&serve_addr), sizeof(serve_addr));
  if (ret != 0) {
    if (errno == EINPROGRESS) {
      LOG_INFO(root_logger) << "connect in progress";
      // 注册监听可写事件，用于判断connect是否成功
      // 非阻塞的TCP Socket的connect一般无法立即建立连接，需要等待一段时间，这段时间内connect会返回EINPROGRESS
      wtsclwq::SockIoScheduler::GetThreadSockIoScheduler()->AddEventListening(sock_fd, wtsclwq::FileDescContext::Write,
                                                                              DoIoWrite);
      // 注册监听可读时间，用于读取数据
      wtsclwq::SockIoScheduler::GetThreadSockIoScheduler()->AddEventListening(sock_fd, wtsclwq::FileDescContext::Read,
                                                                              DoIoRead);
    } else {
      LOG_ERROR(root_logger) << "connect error"
                             << "errno=" << errno << ", errstr = " << strerror(errno);
    }
  } else {
    LOG_INFO(root_logger) << "connect success";
  }
}


auto main() -> int {
  auto sock_io_sc = std::make_shared<wtsclwq::SockIoScheduler>(1, true, "xxx");
  sock_io_sc->Start();
  sock_io_sc->Schedule(std::function<void()>(TestIo));
  sock_io_sc->Stop();
  return 0;
}