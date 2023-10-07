#include <vector>
#include "server/socket.h"
#include "socket_stream.h"

namespace wtsclwq {

SocketStream::SocketStream(SocketWrap::s_ptr socket, bool is_owner) : socket_(std::move(socket)), is_owner_(is_owner) {}

SocketStream::~SocketStream() {
  if (is_owner_ && socket_ != nullptr) {
    socket_->Close();
  }
}

auto SocketStream::Read(void *buffer, size_t length) -> int {
  if (!IsConnected()) {
    return -1;
  }
  return socket_->Recv(buffer, length, 0);
}

auto SocketStream::ReadToByteArray(const ByteArray::s_ptr &ba, size_t length) -> int {
  if (!IsConnected()) {
    return -1;
  }
  std::vector<iovec> iovecs{};
  ba->GetWriteableBuffers(&iovecs, length);
  int ret = socket_->Recv(iovecs.data(), iovecs.size(), 0);
  if (ret > 0) {
    ba->SetPosition(ba->GetPosition() + ret);
  }
  return ret;
}

auto SocketStream::Write(const void *buffer, size_t length) -> int {
  if (!IsConnected()) {
    return -1;
  }
  return socket_->Send(buffer, length, 0);
}

auto SocketStream::WriteFromByteArray(const ByteArray::s_ptr &ba, size_t length) -> int {
  if (!IsConnected()) {
    return -1;
  }
  std::vector<iovec> iovecs{};
  ba->GetReadableBuffers(&iovecs, length);
  int ret = socket_->Send(iovecs.data(), iovecs.size(), 0);
  if (ret > 0) {
    ba->SetPosition(ba->GetPosition() + ret);
  }
  return ret;
}

void SocketStream::Close() {
  if (socket_ != nullptr) {
    socket_->Close();
  }
}

auto SocketStream::GetSocket() -> SocketWrap::s_ptr { return socket_; }

auto SocketStream::IsConnected() -> bool { return socket_ != nullptr && socket_->IsConnected(); }

auto SocketStream::GetRemoteAddress() -> Address::s_ptr {
  if (socket_ == nullptr) {
    return nullptr;
  }
  return socket_->GetRemoteAddress();
}

auto SocketStream::GetLocalAddress() -> Address::s_ptr {
  if (socket_ == nullptr) {
    return nullptr;
  }
  return socket_->GetLocalAddress();
}

auto SocketStream::GetRemoteAddressString() -> std::string {
  auto addr = GetRemoteAddress();
  if (addr != nullptr) {
    return addr->ToString();
  }
  return "";
}

auto SocketStream::GetLocalAddressString() -> std::string {
  auto addr = GetLocalAddress();
  if (addr != nullptr) {
    return addr->ToString();
  }
  return "";
}

}  // namespace wtsclwq