#include <arpa/inet.h>
#include <sys/socket.h>
#include <functional>
#include "server/log.h"
#include "server/server.h"
#include "server/sock_io_scheduler.h"

static auto root_logger = ROOT_LOGGER;

void TestSleep() {
  LOG_INFO(root_logger) << "TestSleep start";
  auto sock_io_scheduler = std::make_shared<wtsclwq::SockIoScheduler>();
  sock_io_scheduler->Start();
  sock_io_scheduler->Schedule(std::function<void()>{[]() {
    LOG_INFO(root_logger) << "before sleep2";
    sleep(2);
    LOG_INFO(root_logger) << "after sleep 2";
  }});
  sock_io_scheduler->Schedule(std::function<void()>([]() {
    LOG_INFO(root_logger) << "before sleep3";
    sleep(3);
    LOG_INFO(root_logger) << "after sleep 3";
  }));
  sock_io_scheduler->Stop();
  LOG_INFO(root_logger) << "TestSleep end";
}

void TestSock() {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(8080);
  inet_pton(AF_INET, "110.242.68.66", &addr.sin_addr.s_addr);
  LOG_INFO(root_logger) << "connect start";
  int ret = connect(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
  LOG_INFO(root_logger) << "connect end"
                        << "ret = " << ret;
  if (ret != 0) {
    LOG_INFO(root_logger) << "connect error";
    return;
  }
  const char data[] = "GET / HTTP/1.0\r\n\r\n";
  ret = send(sock, data, sizeof(data), 0);
  LOG_INFO(root_logger) << "send end"
                        << "ret = " << ret;
  if (ret < 0) {
    LOG_INFO(root_logger) << "send error";
    return;
  }
  std::string buffer;
  buffer.resize(4096);
  ret = recv(sock, buffer.data(), buffer.size(), 0);
  LOG_INFO(root_logger) << "recv end"
                        << "ret = " << ret;
  if (ret < 0) {
    LOG_INFO(root_logger) << "recv error";
    return;
  }
  buffer.resize(ret);
  LOG_INFO(root_logger) << "recv data = " << buffer;
  close(sock);
}

auto main() -> int {
  // TestSleep();
  auto sock_io_scheduler = std::make_shared<wtsclwq::SockIoScheduler>();
  sock_io_scheduler->Start();
  sock_io_scheduler->Schedule(std::function<void()>(TestSock));
  sock_io_scheduler->Stop();
  return 0;
}