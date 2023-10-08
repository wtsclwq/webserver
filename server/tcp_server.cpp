#include "tcp_server.h"

#include <functional>
#include <utility>
#include "server/config.h"
#include "server/log.h"
#include "server/socket.h"

namespace wtsclwq {
static auto sys_logger = NAMED_LOGGER("system");

static auto tcp_server_read_timeout = ConfigMgr::GetInstance()
    -> GetOrAddDefaultConfigItem("tcp_server.read_timeout", 60 * 1000 * 2, "tcp server read timeout");

TcpServer::TcpServer(SockIoScheduler::s_ptr io_scheduler, SockIoScheduler::s_ptr accept_scheduler)
    : io_scheduler_(std::move(io_scheduler)), accept_scheduler_(std::move(accept_scheduler)) {
  read_timeout_ = tcp_server_read_timeout->GetValue();
}

TcpServer::~TcpServer() {
  for (auto &server_socket : server_sockets_) {
    server_socket->Close();
  }
  server_sockets_.clear();
}

auto TcpServer::Start() -> bool {
  if (!stoped_) {
    return true;
  }
  stoped_ = false;
  for (auto &server_socket : server_sockets_) {
    accept_scheduler_->Schedule(
        std::function<void()>([this, server_socket]() { OneServerSocketStartAccept(server_socket); }));
  }
  return true;
}

void TcpServer::Stop() {
  stoped_ = true;
  auto self = shared_from_this();
  accept_scheduler_->Schedule(std::function<void()>([this, self]() {
    for (auto &server_socket : server_sockets_) {
      server_socket->RemoveAndTryTriggerAll();
      server_socket->Close();
    }
  }));
}

auto TcpServer::BindServerAddr(Address::s_ptr addr) -> bool {
  std::vector<Address::s_ptr> addr_vec{std::move(addr)};
  std::vector<Address::s_ptr> fails{};
  return BindServerAddrVec(addr_vec, &fails);
}

auto TcpServer::BindServerAddrVec(const std::vector<Address::s_ptr> &addr_vec, std::vector<Address::s_ptr> *fails)
    -> bool {
  for (auto &addr : addr_vec) {
    auto server_socket = SocketWrap::CreateTcpSocket(addr);
    if (!server_socket->Bind(addr)) {
      LOG_ERROR(sys_logger) << "bind server addr failed, addr: " << addr->ToString();
      fails->push_back(addr);
      continue;
    }
    if (!server_socket->Listen(SOMAXCONN)) {
      LOG_ERROR(sys_logger) << "listen server addr failed, addr: " << addr->ToString();
      fails->push_back(addr);
      continue;
    }
    server_sockets_.push_back(server_socket);
  }
  if (!fails->empty()) {
    server_sockets_.clear();
    return false;
  }
  for (auto &server_socket : server_sockets_) {
    LOG_INFO(sys_logger) << "bind server addr success, addr: " << server_socket->GetLocalAddress()->ToString();
  }
  return true;
}

auto TcpServer::OneServerSocketStartAccept(const SocketWrap::s_ptr &server_socket) -> void {
  while (!IsStoped()) {
    auto client_socket = server_socket->Accept();
    if (!client_socket) {
      LOG_ERROR(sys_logger) << "accept client socket failed, server addr: "
                            << server_socket->GetLocalAddress()->ToString();
      continue;
    }
    LOG_INFO(sys_logger) << "accept client socket success, server addr: "
                         << server_socket->GetLocalAddress()->ToString()
                         << ", client addr: " << client_socket->GetRemoteAddress()->ToString();
    client_socket->SetReadTimeout(read_timeout_);
    io_scheduler_->Schedule(std::function<void()>([this, client_socket]() { HandleAccept(client_socket); }));
  }
}

auto TcpServer::GetReadTimeout() const -> int64_t { return read_timeout_; }

auto TcpServer::SetReadTimeout(int64_t timeout) -> void { read_timeout_ = timeout; }

auto TcpServer::GetName() const -> std::string { return name_; }

auto TcpServer::SetName(const std::string &name) -> void { name_ = name; }

auto TcpServer::IsStoped() const -> bool { return stoped_; }

auto TcpServer::ToString(std::string_view prefix) -> std::string {
  std::stringstream ss;
  ss << prefix << "TcpServer[" << name_ << "]: " << std::endl;
  ss << prefix << "  type: " << type_ << std::endl;
  ss << prefix << "  read_timeout: " << read_timeout_ << std::endl;
  ss << prefix << "  stoped: " << stoped_ << std::endl;
  ss << prefix << "  server_sockets: " << std::endl;
  for (auto &server_socket : server_sockets_) {
    ss << (prefix.empty() ? "   " : prefix) << server_socket->ToString() << std::endl;
  }
  return ss.str();
}
}  // namespace wtsclwq