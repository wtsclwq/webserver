#include <sys/socket.h>
#include <functional>
#include <memory>
#include <string>
#include "server/address.h"
#include "server/log.h"
#include "server/server.h"
#include "server/sock_io_scheduler.h"
#include "server/socket.h"

auto root_logger = ROOT_LOGGER;

void TestSocket0TcpServer() {
  auto addr = wtsclwq::Address::GetAnyOneIPByHost("127.0.0.1:9001");
  ASSERT(addr != nullptr);

  auto server_socket = wtsclwq::SocketWrap::CreateTcpSocketV4();
  ASSERT(server_socket != nullptr);

  bool bind_success = server_socket->Bind(addr);
  ASSERT(bind_success);

  bool listen_success = server_socket->Listen(SOMAXCONN);
  ASSERT(listen_success);

  LOG_INFO(root_logger) << "server listen on " << server_socket->ToString();
  LOG_INFO(root_logger) << "Accepting...";

  int i = 0;
  while (true) {
    auto client_socket = server_socket->Accept();
    if (client_socket == nullptr) {
      LOG_ERROR(root_logger) << "Accept failed";
      continue;
    }

    LOG_INFO(root_logger) << "Accept a new connection from " << client_socket->ToString();
    std::string msg = "Hello, world!" + std::to_string(i++);
    client_socket->Send(msg.data(), msg.size(), 0);
    client_socket->Close();
  }
}

auto main() -> int {
  auto sock_io_scheduler = std::make_shared<wtsclwq::SockIoScheduler>(2);
  sock_io_scheduler->Start();
  sock_io_scheduler->Schedule(std::function<void()>(TestSocket0TcpServer));
  sock_io_scheduler->Stop();
  return 0;
}
