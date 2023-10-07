#include "socket.h"
#include <bits/types/struct_iovec.h>
#include <bits/types/struct_timeval.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include "log.h"
#include "server/address.h"
#include "server/fd_context.h"
#include "server/fd_manager.h"
#include "server/hook.h"
#include "server/sock_io_scheduler.h"
#include "server/utils.h"

namespace wtsclwq {

static auto sys_logger = NAMED_LOGGER("sys");

SocketWrap::SocketWrap(int family, int type, int protocol) : family_(family), type_(type), protocol_(protocol) {}

SocketWrap::~SocketWrap() { Close(); }

void SocketWrap::InitSelf() {
  int opt = 1;
  // 设置地址复用
  SetSocketOption(SOL_SOCKET, SO_REUSEADDR, opt);
  if (type_ == SocketType::TCP) {
    // 设置地址复用
    SetSocketOption(IPPROTO_TCP, TCP_NODELAY, opt);
  }
}

auto SocketWrap::InitFromSocketFd(int socket) -> bool {
  auto ctx = FdWrapperMgr::GetInstance()->Get(socket);
  if (ctx == nullptr || !ctx->IsSocket() || ctx->IsClosed()) {
    LOG_ERROR(sys_logger) << "Invalid socket fd: " << socket;
    return false;
  }
  sys_sock_ = socket;
  is_connected_ = true;
  InitSelf();
  InitLocalAddress();
  InitRemoteAddress();
  return true;
}

auto SocketWrap::CreateTcpSocket(const Address::s_ptr &address) -> SocketWrap::s_ptr {
  return std::make_shared<SocketWrap>(address->GetFamily(), SocketType::TCP, 0);
}

auto SocketWrap::CreateUdpSocket(const Address::s_ptr &address) -> SocketWrap::s_ptr {
  auto res = std::make_shared<SocketWrap>(address->GetFamily(), SocketType::UDP, 0);
  res->ApplyNewSocketFd();
  res->is_connected_ = true;
  return res;
}

auto SocketWrap::CreateTcpSocketV4() -> SocketWrap::s_ptr {
  return std::make_shared<SocketWrap>(Family::IPv4, SocketType::TCP, 0);
}

auto SocketWrap::CreateUdpSocketV4() -> SocketWrap::s_ptr {
  auto res = std::make_shared<SocketWrap>(Family::IPv4, SocketType::UDP, 0);
  res->ApplyNewSocketFd();
  res->is_connected_ = true;
  return res;
}

auto SocketWrap::CreateTcpSocketV6() -> SocketWrap::s_ptr {
  return std::make_shared<SocketWrap>(Family::IPv6, SocketType::TCP, 0);
}

auto SocketWrap::CreateUdpSocketV6() -> SocketWrap::s_ptr {
  auto res = std::make_shared<SocketWrap>(Family::IPv6, SocketType::UDP, 0);
  res->ApplyNewSocketFd();
  res->is_connected_ = true;
  return res;
}

auto SocketWrap::CreateTCPSocketUnix() -> SocketWrap::s_ptr {
  return std::make_shared<SocketWrap>(Family::UNIX, SocketType::TCP, 0);
}

auto SocketWrap::CreateUDPSocketUnix() -> SocketWrap::s_ptr {
  auto res = std::make_shared<SocketWrap>(Family::UNIX, SocketType::UDP, 0);
  res->ApplyNewSocketFd();
  res->is_connected_ = true;
  return res;
}

void SocketWrap::ApplyNewSocketFd() {
  sys_sock_ = socket(family_, type_, protocol_);
  if (sys_sock_ == -1) {
    LOG_ERROR(sys_logger) << "socket() failed: " << strerror(errno);
  } else {
    InitSelf();
  }
}

auto SocketWrap::GetWritemeout() -> uint64_t {
  auto ctx = FdWrapperMgr::GetInstance()->Get(sys_sock_);
  if (ctx == nullptr) {
    return UINT64_MAX;
  }
  return ctx->GetTimeout(SO_SNDTIMEO);
}

void SocketWrap::SetWriteTimeout(uint64_t v) {
  struct timeval tv {
    static_cast<time_t>(v / 1000), static_cast<suseconds_t>((v % 1000) * 1000)
  };
  // 为什么不需要设置fdmgr的timeout?
  // 因为SetSocketOption会调用被hook了的setsockopt, 其中会自动设置fdmgr的timeout
  SetSocketOption(SOL_SOCKET, SO_SNDTIMEO, tv);
}

auto SocketWrap::GetReadTimeout() -> uint64_t {
  auto ctx = FdWrapperMgr::GetInstance()->Get(sys_sock_);
  if (ctx == nullptr) {
    return UINT64_MAX;
  }
  return ctx->GetTimeout(SO_RCVTIMEO);
}

void SocketWrap::SetReadTimeout(uint64_t v) {
  struct timeval tv {
    static_cast<time_t>(v / 1000), static_cast<suseconds_t>((v % 1000) * 1000)
  };
  // 为什么不需要设置fdmgr的timeout?
  // 因为SetSocketOption会调用被hook了的setsockopt, 其中会自动设置fdmgr的timeout
  SetSocketOption(SOL_SOCKET, SO_RCVTIMEO, tv);
}

auto SocketWrap::GetSocketOption(int level, int option, void *result, socklen_t *len) -> bool {
  int ret = getsockopt(sys_sock_, level, option, result, len);
  if (ret != 0) {
    LOG_ERROR(sys_logger) << "getsockopt() failed: " << strerror(errno);
    return false;
  }
  return true;
}

auto SocketWrap::SetSocketOption(int level, int option, const void *result, socklen_t len) -> bool {
  int ret = setsockopt(sys_sock_, level, option, result, len);
  if (ret != 0) {
    LOG_ERROR(sys_logger) << "setsockopt() failed: " << strerror(errno);
    return false;
  }
  return true;
}

auto SocketWrap::Accept() -> SocketWrap::s_ptr {
  // 和server段socket相同类型的socket
  auto res = std::make_shared<SocketWrap>(family_, type_, protocol_);
  // 为新的socket分配fd
  int new_socket_fd = accept(sys_sock_, nullptr, nullptr);
  if (new_socket_fd == -1) {
    LOG_ERROR(sys_logger) << "accept() failed: " << strerror(errno);
    return nullptr;
  }
  if (res->InitFromSocketFd(new_socket_fd)) {
    return res;
  }
  return nullptr;
}

auto SocketWrap::Bind(Address::s_ptr addr) -> bool {
  local_address_ = std::move(addr);
  if (!IsValid()) {
    ApplyNewSocketFd();
    if (!IsValid()) {
      return false;
    }
  }

  if (local_address_->GetFamily() != family_) {
    LOG_ERROR(sys_logger) << "Bind address family not equal socket family";
    return false;
  }

  // 如果地址是一个unix地址, 那么需要先unlink
  auto unix_addr = std::dynamic_pointer_cast<UnixAddress>(local_address_);
  if (unix_addr != nullptr) {
    auto socket = CreateTCPSocketUnix();
    if (socket->Connect(unix_addr, UINT64_MAX)) {
      return false;
    }
    FSUtil::Unlink(unix_addr->GetPath(), true);
  }

  if (bind(sys_sock_, local_address_->GetSockAddr(), local_address_->GetSockAddrLen()) != 0) {
    LOG_ERROR(sys_logger) << "bind() failed: " << strerror(errno);
    return false;
  }
  InitLocalAddress();
  return true;
}

auto SocketWrap::Connect(Address::s_ptr addr, uint64_t timeout_ms) -> bool {
  remote_address_ = std::move(addr);
  if (!IsValid()) {
    ApplyNewSocketFd();
    if (!IsValid()) {
      return false;
    }
  }
  if (remote_address_->GetFamily() != family_) {
    LOG_ERROR(sys_logger) << "Connect address family not equal socket family";
    return false;
  }

  if (timeout_ms == UINT64_MAX) {
    if (connect(sys_sock_, remote_address_->GetSockAddr(), remote_address_->GetSockAddrLen()) != 0) {
      LOG_ERROR(sys_logger) << "connect() failed: " << strerror(errno);
      Close();
      return false;
    }
  } else {
    if (ConnectWithTimeout(sys_sock_, remote_address_->GetSockAddr(), remote_address_->GetSockAddrLen(), timeout_ms) ==
        0) {
      LOG_ERROR(sys_logger) << "connect_with_timeout() failed: " << strerror(errno) << " timeout_ms: " << timeout_ms;
      Close();
      return false;
    }
  }

  is_connected_ = true;
  InitLocalAddress();
  InitRemoteAddress();
  return true;
}

auto SocketWrap::ReConnect(uint64_t timeout_ms) -> bool {
  if (remote_address_ == nullptr) {
    LOG_ERROR(sys_logger) << "ReConnect() failed, remote_address_ is nullptr";
    return false;
  }
  return Connect(remote_address_, timeout_ms);
}

auto SocketWrap::Listen(int backlog) -> bool {
  if (!IsValid()) {
    LOG_ERROR(sys_logger) << "Listen() failed, socket is invalid";
    return false;
  }

  if (listen(sys_sock_, backlog) != 0) {
    LOG_ERROR(sys_logger) << "listen() failed: " << strerror(errno);
    return false;
  }
  return true;
}

auto SocketWrap::Close() -> bool {
  if (!is_connected_ && sys_sock_ == -1) {
    return true;
  }
  is_connected_ = false;
  if (sys_sock_ != -1) {
    close(sys_sock_);
    sys_sock_ = -1;
  }
  return false;
}

auto SocketWrap::Send(const void *buffer, size_t length, int flags) -> int {
  if (!is_connected_) {
    LOG_ERROR(sys_logger) << "Send() failed, socket is not connected";
    return -1;
  }
  int ret = send(sys_sock_, buffer, length, flags);
  if (ret == -1) {
    LOG_ERROR(sys_logger) << "send() failed: " << strerror(errno);
  }
  return ret;
}

auto SocketWrap::Send(const iovec *buffers, size_t length, int flags) -> int {
  if (!is_connected_) {
    LOG_ERROR(sys_logger) << "Send() failed, socket is not connected";
    return -1;
  }
  msghdr msg{};
  msg.msg_iov = const_cast<iovec *>(buffers);
  msg.msg_iovlen = length;
  int ret = sendmsg(sys_sock_, &msg, flags);
  if (ret == -1) {
    LOG_ERROR(sys_logger) << "sendmsg() failed: " << strerror(errno);
  }
  return ret;
}

auto SocketWrap::SendTo(const void *buffer, size_t length, const Address::s_ptr &to, int flags) -> int {
  if (!is_connected_) {
    LOG_ERROR(sys_logger) << "SendTo() failed, socket is not connected";
    return -1;
  }
  int ret = sendto(sys_sock_, buffer, length, flags, to->GetSockAddr(), to->GetSockAddrLen());
  if (ret == -1){
    LOG_ERROR(sys_logger) << "sendto() failed: " << strerror(errno);
  }
  return ret;
}

auto SocketWrap::SendTo(const iovec *buffers, size_t length, const Address::s_ptr &to, int flags) -> int {
  if (!is_connected_) {
    LOG_ERROR(sys_logger) << "SendTo() failed, socket is not connected";
    return -1;
  }

  msghdr msg{};
  msg.msg_iov = const_cast<iovec *>(buffers);
  msg.msg_iovlen = length;
  msg.msg_name = to->GetSockAddr();
  msg.msg_namelen = to->GetSockAddrLen();

  int ret = sendmsg(sys_sock_, &msg, flags);
  if (ret == -1) {
    LOG_ERROR(sys_logger) << "sendmsg() failed: " << strerror(errno);
  }
  return ret;
}

auto SocketWrap::Recv(void *buffer, size_t length, int flags) -> int {
  if (!is_connected_) {
    LOG_ERROR(sys_logger) << "Recv() failed, socket is not connected";
    return -1;
  }

  int ret = recv(sys_sock_, buffer, length, flags);
  if (ret == -1) {
    LOG_ERROR(sys_logger) << "recv() failed: " << strerror(errno);
  }
  return ret;
}

auto SocketWrap::Recv(iovec *buffers, size_t length, int flags) -> int {
  if (!is_connected_) {
    LOG_ERROR(sys_logger) << "Recv() failed, socket is not connected";
    return -1;
  }

  msghdr msg{};
  msg.msg_iov = buffers;
  msg.msg_iovlen = length;

  int ret = recvmsg(sys_sock_, &msg, flags);
  if (ret == -1) {
    LOG_ERROR(sys_logger) << "recvmsg() failed: " << strerror(errno);
  }
  return ret;
}

auto SocketWrap::RecvFrom(void *buffer, size_t length, const Address::s_ptr &from, int flags) -> int {
  if (!is_connected_) {
    LOG_ERROR(sys_logger) << "RecvFrom() failed, socket is not connected";
    return -1;
  }
  socklen_t len = from->GetSockAddrLen();
  int ret = recvfrom(sys_sock_, buffer, length, flags, from->GetSockAddr(), &len);
  if (ret == -1) {
    LOG_ERROR(sys_logger) << "recvfrom() failed: " << strerror(errno);
  }
  return ret;
}

auto SocketWrap::RecvFrom(iovec *buffers, size_t length, const Address::s_ptr &from, int flags) -> int {
  if (!is_connected_) {
    LOG_ERROR(sys_logger) << "RecvFrom() failed, socket is not connected";
    return -1;
  }

  msghdr msg{};
  msg.msg_iov = buffers;
  msg.msg_iovlen = length;
  msg.msg_name = from->GetSockAddr();
  msg.msg_namelen = from->GetSockAddrLen();

  int ret = recvmsg(sys_sock_, &msg, flags);
  if (ret == -1) {
    LOG_ERROR(sys_logger) << "recvmsg() failed: " << strerror(errno);
  }
  return ret;
}

auto SocketWrap::GetLocalAddress() -> Address::s_ptr {
  if (local_address_ != nullptr) {
    return local_address_;
  }
  return InitLocalAddress();
}

auto SocketWrap::GetRemoteAddress() -> Address::s_ptr {
  if (remote_address_ != nullptr) {
    return remote_address_;
  }
  return InitRemoteAddress();
}

auto SocketWrap::InitLocalAddress() -> Address::s_ptr {
  Address::s_ptr res = nullptr;
  switch (family_) {
    case AF_INET: {
      res = std::make_shared<IPv4Address>();
      break;
    }
    case AF_INET6: {
      res = std::make_shared<IPv6Address>();
      break;
    }
    case AF_UNIX: {
      res = std::make_shared<UnixAddress>();
      break;
    }
    default:
      res = std::make_shared<UnknownAddress>(family_);
      LOG_ERROR(sys_logger) << "InitLocalAddress() failed, unknown address family: " << family_;
  }
  socklen_t len = res->GetSockAddrLen();
  if (getsockname(sys_sock_, res->GetSockAddr(), &len) != 0) {
    LOG_ERROR(sys_logger) << "getsockname() failed: " << strerror(errno);
    return std::make_shared<UnknownAddress>(family_);
  }
  if (family_ == AF_UNIX) {
    auto unix_addr = std::dynamic_pointer_cast<UnixAddress>(res);
    if (unix_addr != nullptr) {
      unix_addr->SetAddrlen(len);
    }
  }

  local_address_ = res;
  return res;
}

auto SocketWrap::InitRemoteAddress() -> Address::s_ptr {
  Address::s_ptr res = nullptr;
  switch (family_) {
    case AF_INET: {
      res = std::make_shared<IPv4Address>();
      break;
    }
    case AF_INET6: {
      res = std::make_shared<IPv6Address>();
      break;
    }
    case AF_UNIX: {
      res = std::make_shared<UnixAddress>();
      break;
    }
    default:
      res = std::make_shared<UnknownAddress>(family_);
      LOG_ERROR(sys_logger) << "InitRemoteAddress() failed, unknown address family: " << family_;
  }
  socklen_t len = res->GetSockAddrLen();
  if (getpeername(sys_sock_, res->GetSockAddr(), &len) != 0) {
    LOG_ERROR(sys_logger) << "getpeername() failed: " << strerror(errno);
    return std::make_shared<UnknownAddress>(family_);
  }
  if (family_ == AF_UNIX) {
    auto unix_addr = std::dynamic_pointer_cast<UnixAddress>(res);
    if (unix_addr != nullptr) {
      unix_addr->SetAddrlen(len);
    }
  }

  remote_address_ = res;
  return res;
}

auto SocketWrap::IsValid() const -> bool { return sys_sock_ != -1; }

auto SocketWrap::GetSocketError() -> int {
  int err = 0;
  socklen_t len = sizeof(err);
  if (!GetSocketOption(SOL_SOCKET, SO_ERROR, &err, &len)) {
    return errno;
  }
  return err;
}

auto SocketWrap::Dump(std::ostream &os) const -> std::ostream & {
  os << "[SocketWrap sock=" << sys_sock_ << " is_connected=" << is_connected_ << " family=" << family_
     << " type=" << type_ << " protocol=" << protocol_
     << " local_address=" << (local_address_ ? local_address_->ToString() : "nullptr")
     << " remote_address=" << (remote_address_ ? remote_address_->ToString() : "nullptr") << "]";
  return os;
}

auto SocketWrap::ToString() const -> std::string {
  std::stringstream ss;
  Dump(ss);
  return ss.str();
}

auto SocketWrap::CancelAndTryTriggerRead() -> bool {
  return SockIoScheduler::GetThreadSockIoScheduler()->RemoveAndTriggerEventListening(sys_sock_,
                                                                                     FileDescContext::EventType::Read);
}

auto SocketWrap::RemoveAndTryTriggerWrite() -> bool {
  return SockIoScheduler::GetThreadSockIoScheduler()->RemoveAndTriggerEventListening(sys_sock_,
                                                                                     FileDescContext::EventType::Write);
}

auto SocketWrap::RemoveAndTryTriggerAccept() -> bool {
  return SockIoScheduler::GetThreadSockIoScheduler()->RemoveEventListening(sys_sock_, FileDescContext::EventType::Read);
}

auto SocketWrap::RemoveAndTryTriggerAll() -> bool {
  return SockIoScheduler::GetThreadSockIoScheduler()->RemoveAndTriggerAllTypeEventListening(sys_sock_);
}

auto operator<<(std::ostream &os, const SocketWrap &sock) -> std::ostream & { return sock.Dump(os); }

}  // namespace wtsclwq