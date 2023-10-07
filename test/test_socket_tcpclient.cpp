#include "server/log.h"
#include "server/server.h"

auto root_logger = ROOT_LOGGER;

void TestSocketTcpClient() {
  for (int i = 0; i < 10; i++) {
    auto client_socket = wtsclwq::SocketWrap::CreateTcpSocketV4();
    ASSERT(client_socket != nullptr);

    auto addr = wtsclwq::Address::GetAnyOneAddrByHost("127.0.0.1:9001");
    ASSERT(addr != nullptr);

    bool connect_success = client_socket->Connect(addr, 0);
    ASSERT(connect_success);

    LOG_INFO(root_logger) << "Connect to " << client_socket->ToString();
    std::string buffer(1024, 0);
    int recv_len = client_socket->Recv(buffer.data(), 1024, 0);
    LOG_INFO(root_logger) << "Recv " << recv_len << " bytes: " << buffer;
    client_socket->Close();
  }
}

auto main() -> int {
  auto sock_io_scheduler = std::make_shared<wtsclwq::SockIoScheduler>(1);
  sock_io_scheduler->Start();
  sock_io_scheduler->Schedule(std::function<void()>(TestSocketTcpClient));
  sock_io_scheduler->Stop();
  return 0;
}