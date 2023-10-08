#ifndef _TCP_SERVER_H_
#define _TCP_SERVER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "server/address.h"
#include "server/noncopyable.h"
#include "server/sock_io_scheduler.h"
#include "server/socket.h"
#include "server/thread.h"
namespace wtsclwq {
class TcpServer : public std::enable_shared_from_this<TcpServer>, Noncopyable {
 public:
  using s_ptr = std::shared_ptr<TcpServer>;

  explicit TcpServer(SockIoScheduler::s_ptr io_scheduler = SockIoScheduler::GetThreadSockIoScheduler(),
                     SockIoScheduler::s_ptr accept_scheduler = SockIoScheduler::GetThreadSockIoScheduler());

  ~TcpServer() override;

  virtual auto Start() -> bool;

  virtual void Stop();

  virtual auto BindServerAddr(Address::s_ptr addr) -> bool;

  virtual auto BindServerAddrVec(const std::vector<Address::s_ptr> &addr_vec, std::vector<Address::s_ptr> *fails)
      -> bool;

  auto GetReadTimeout() const -> int64_t;

  auto SetReadTimeout(int64_t timeout) -> void;

  auto GetName() const -> std::string;

  auto SetName(const std::string &name) -> void;

  auto IsStoped() const -> bool;

  auto ToString(std::string_view prefix = "") -> std::string;

 protected:
  virtual void HandleAccept(SocketWrap::s_ptr client_socket);

  virtual void OneServerSocketStartAccept(const SocketWrap::s_ptr &server_socket);

  // 所有被监听的服务端Socket
  std::vector<SocketWrap::s_ptr> server_sockets_{};
  // 负责调度业务处理中的IO请求
  SockIoScheduler::s_ptr io_scheduler_{};
  // 负责调度服务端Socket的Accept请求
  SockIoScheduler::s_ptr accept_scheduler_{};
  // 服务端Socket的读超时时间
  int64_t read_timeout_{};
  // 服务器名称
  std::string name_{"wrsclwq-server"};
  // 服务器类型
  std::string type_{"tcp"};
  // 是否停止
  bool stoped_{};
};

}  // namespace wtsclwq

#endif  // _TCP_SERVER_H_