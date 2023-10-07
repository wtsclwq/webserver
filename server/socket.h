/**
 * @file socket.h
 * @brief Socket封装
 * @author wtsclwq.yin
 * @email 564628276@qq.com
 * @date 2019-06-05
 * @copyright Copyright (c) 2019年 wtsclwq.yin All rights reserved (www.wtsclwq.top)
 */
#ifndef __wtsclwq_SOCKET_H__
#define __wtsclwq_SOCKET_H__

#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <cstdint>
#include <memory>
#include "address.h"
#include "noncopyable.h"

namespace wtsclwq {

/**
 * @brief Socket封装类
 */
class SocketWrap : public std::enable_shared_from_this<SocketWrap>, Noncopyable {
 public:
  using s_ptr = std::shared_ptr<SocketWrap>;
  using w_ptr = std::weak_ptr<SocketWrap>;

  /**
   * @brief Socket类型
   */
  enum SocketType {
    /// TCP类型
    TCP = SOCK_STREAM,
    /// UDP类型
    UDP = SOCK_DGRAM
  };

  /**
   * @brief Socket协议簇
   */
  enum Family {
    /// IPv4 socket
    IPv4 = AF_INET,
    /// IPv6 socket
    IPv6 = AF_INET6,
    /// Unix socket
    UNIX = AF_UNIX,
  };

  /**
   * @brief 构造函数
   * @param[in] family 协议簇
   * @param[in] type 类型
   * @param[in] protocol 协议
   */
  SocketWrap(int family, int type, int protocol = 0);

  /**
   * @brief 析构函数
   */
  ~SocketWrap() override;

  /**
   * @brief 创建TCP Socket(满足地址类型)
   * @param[in] address 地址
   */
  static auto CreateTcpSocket(const Address::s_ptr &address) -> SocketWrap::s_ptr;

  /**
   * @brief 创建UDP Socket(满足地址类型)
   * @param[in] address 地址
   */
  static auto CreateUdpSocket(const Address::s_ptr &address) -> SocketWrap::s_ptr;

  /**
   * @brief 创建IPv4的TCP Socket
   */
  static auto CreateTcpSocketV4() -> SocketWrap::s_ptr;

  /**
   * @brief 创建IPv4的UDP Socket
   */
  static auto CreateUdpSocketV4() -> SocketWrap::s_ptr;

  /**
   * @brief 创建IPv6的TCP Socket
   */
  static auto CreateTcpSocketV6() -> SocketWrap::s_ptr;

  /**
   * @brief 创建IPv6的UDP Socket
   */
  static auto CreateUdpSocketV6() -> SocketWrap::s_ptr;

  /**
   * @brief 创建Unix的TCP Socket
   */
  static auto CreateTCPSocketUnix() -> SocketWrap::s_ptr;

  /**
   * @brief 创建Unix的UDP Socket
   */
  static auto CreateUDPSocketUnix() -> SocketWrap::s_ptr;

  /**
   * @brief 获取发送超时时间(毫秒)
   */
  auto GetWritemeout() -> uint64_t;

  /**
   * @brief 设置发送超时时间(毫秒)
   */
  void SetWriteTimeout(uint64_t v);

  /**
   * @brief 获取接受超时时间(毫秒)
   */
  auto GetReadTimeout() -> uint64_t;

  /**
   * @brief 设置接受超时时间(毫秒)
   */
  void SetReadTimeout(uint64_t v);

  /**
   * @brief 获取sockopt @see getsockopt
   */
  auto GetSocketOption(int level, int option, void *result, socklen_t *len) -> bool;

  /**
   * @brief 获取sockopt模板 @see getsockopt
   */
  template <class T>
  auto GetSocketOption(int level, int option, T &result) -> bool {
    socklen_t length = sizeof(T);
    return GetSocketOption(level, option, &result, &length);
  }

  /**
   * @brief 设置sockopt @see setsockopt
   */
  auto SetSocketOption(int level, int option, const void *result, socklen_t len) -> bool;

  /**
   * @brief 设置sockopt模板 @see setsockopt
   */
  template <class T>
  auto SetSocketOption(int level, int option, const T &value) -> bool {
    return SetSocketOption(level, option, &value, sizeof(T));
  }

  /**
   * @brief 接收connect链接
   * @return 成功返回新连接的socket,失败返回nullptr
   * @pre Socket必须 bind , listen  成功
   */
  virtual auto Accept() -> SocketWrap::s_ptr;

  /**
   * @brief 绑定地址
   * @param[in] addr 地址
   * @return 是否绑定成功
   */
  virtual auto Bind(Address::s_ptr addr) -> bool;

  /**
   * @brief 连接地址
   * @param[in] addr 目标地址
   * @param[in] timeout_ms 超时时间(毫秒)
   */
  virtual auto Connect(Address::s_ptr addr, uint64_t timeout_ms) -> bool;

  virtual auto ReConnect(uint64_t timeout_ms) -> bool;

  /**
   * @brief 监听socket
   * @param[in] backlog 未完成连接队列的最大长度
   * @result 返回监听是否成功
   * @pre 必须先 bind 成功
   */
  virtual auto Listen(int backlog) -> bool;

  /**
   * @brief 关闭socket
   */
  virtual auto Close() -> bool;

  /**
   * @brief 发送数据
   * @param[in] buffer 待发送数据的内存
   * @param[in] length 待发送数据的长度
   * @param[in] flags 标志字
   * @return
   *      @retval >0 发送成功对应大小的数据
   *      @retval =0 socket被关闭
   *      @retval <0 socket出错
   */
  virtual auto Send(const void *buffer, size_t length, int flags) -> int;

  /**
   * @brief 发送数据
   * @param[in] buffers 待发送数据的内存(iovec数组)
   * @param[in] length 待发送数据的长度(iovec长度)
   * @param[in] flags 标志字
   * @return
   *      @retval >0 发送成功对应大小的数据
   *      @retval =0 socket被关闭
   *      @retval <0 socket出错
   */
  virtual auto Send(const iovec *buffers, size_t length, int flags) -> int;

  /**
   * @brief 发送数据
   * @param[in] buffer 待发送数据的内存
   * @param[in] length 待发送数据的长度
   * @param[in] to 发送的目标地址
   * @param[in] flags 标志字
   * @return
   *      @retval >0 发送成功对应大小的数据
   *      @retval =0 socket被关闭
   *      @retval <0 socket出错
   */
  virtual auto SendTo(const void *buffer, size_t length, const Address::s_ptr &to, int flags) -> int;

  /**
   * @brief 发送数据
   * @param[in] buffers 待发送数据的内存(iovec数组)
   * @param[in] length 待发送数据的长度(iovec长度)
   * @param[in] to 发送的目标地址
   * @param[in] flags 标志字
   * @return
   *      @retval >0 发送成功对应大小的数据
   *      @retval =0 socket被关闭
   *      @retval <0 socket出错
   */
  virtual auto SendTo(const iovec *buffers, size_t length, const Address::s_ptr &to, int flags) -> int;

  /**
   * @brief 接受数据
   * @param[out] buffer 接收数据的内存
   * @param[in] length 接收数据的内存大小
   * @param[in] flags 标志字
   * @return
   *      @retval >0 接收到对应大小的数据
   *      @retval =0 socket被关闭
   *      @retval <0 socket出错
   */
  virtual auto Recv(void *buffer, size_t length, int flags) -> int;

  /**
   * @brief 接受数据
   * @param[out] buffers 接收数据的内存(iovec数组)
   * @param[in] length 接收数据的内存大小(iovec数组长度)
   * @param[in] flags 标志字
   * @return
   *      @retval >0 接收到对应大小的数据
   *      @retval =0 socket被关闭
   *      @retval <0 socket出错
   */
  virtual auto Recv(iovec *buffers, size_t length, int flags) -> int;

  /**
   * @brief 接受数据
   * @param[out] buffer 接收数据的内存
   * @param[in] length 接收数据的内存大小
   * @param[out] from 发送端地址
   * @param[in] flags 标志字
   * @return
   *      @retval >0 接收到对应大小的数据
   *      @retval =0 socket被关闭
   *      @retval <0 socket出错
   */
  virtual auto RecvFrom(void *buffer, size_t length, const Address::s_ptr &from, int flags) -> int;

  /**
   * @brief 接受数据
   * @param[out] buffers 接收数据的内存(iovec数组)
   * @param[in] length 接收数据的内存大小(iovec数组长度)
   * @param[out] from 发送端地址
   * @param[in] flags 标志字
   * @return
   *      @retval >0 接收到对应大小的数据
   *      @retval =0 socket被关闭
   *      @retval <0 socket出错
   */
  virtual auto RecvFrom(iovec *buffers, size_t length, const Address::s_ptr &from, int flags) -> int;

  /**
   * @brief 初始化远端地址
   */
  auto InitRemoteAddress() -> Address::s_ptr;

  /**
   * @brief 初始化本地地址
   */
  auto InitLocalAddress() -> Address::s_ptr;

  /**
   * @brief 获取远端地址
   */
  auto GetRemoteAddress() -> Address::s_ptr;

  /**
   * @brief 获取本地地址
   */
  auto GetLocalAddress() -> Address::s_ptr;

  /**
   * @brief 获取协议簇
   */
  auto GetFamily() const -> int { return family_; }

  /**
   * @brief 获取类型
   */
  auto GetType() const -> int { return type_; }

  /**
   * @brief 获取协议
   */
  auto GetProtocol() const -> int { return protocol_; }

  /**
   * @brief 返回是否连接
   */
  auto IsConnected() const -> bool { return is_connected_; }

  /**
   * @brief 是否有效(m_sock != -1)
   */
  auto IsValid() const -> bool;

  /**
   * @brief 返回Socket错误
   */
  auto GetSocketError() -> int;

  /**
   * @brief 输出信息到流中
   */
  virtual auto Dump(std::ostream &os) const -> std::ostream &;

  virtual auto ToString() const -> std::string;

  /**
   * @brief 返回socket句柄
   */
  auto GetSocket() const -> int { return sys_sock_; }

  /**
   * @brief 取消读
   */
  auto CancelAndTryTriggerRead() -> bool;

  /**
   * @brief 取消写
   */
  auto RemoveAndTryTriggerWrite() -> bool;

  /**
   * @brief 取消accept
   */
  auto RemoveAndTryTriggerAccept() -> bool;

  /**
   * @brief 取消所有事件
   */
  auto RemoveAndTryTriggerAll() -> bool;

 protected:
  /**
   * @brief 初始化socket
   */
  void InitSelf();

  /**
   * @brief 创建socket
   */
  void ApplyNewSocketFd();

  /**
   * @brief 初始化sock
   */
  virtual auto InitFromSocketFd(int sock) -> bool;

 protected:
  /// socket句柄
  int sys_sock_{-1};
  /// 协议簇
  int family_{};
  /// 类型
  int type_{};
  /// 协议
  int protocol_{};
  /// 是否连接
  bool is_connected_{false};
  /// 本地地址
  Address::s_ptr local_address_{};
  /// 远端地址
  Address::s_ptr remote_address_{};
};

/**
 * @brief 流式输出socket
 * @param[in, out] os 输出流
 * @param[in] sock Socket类
 */
auto operator<<(std::ostream &os, const SocketWrap &sock) -> std::ostream &;

}  // namespace wtsclwq

#endif
