#ifndef _ADDRESS_H_
#define _ADDRESS_H_

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cstdint>
#include <map>
#include <memory>
#include <ostream>
#include <string_view>
#include <utility>
#include <vector>

namespace wtsclwq {
class IPAddress;

class Address {
 public:
  using s_ptr = std::shared_ptr<Address>;

  virtual ~Address();

  /**
   * @brief 根据sockaddr和socklen_t创建Address指针
   */
  static auto CreateAddr(const sockaddr *addr, socklen_t addr_len) -> s_ptr;

  /**
   * @brief 根据host获取满足对应条件的所有地址
   * @param host 域名、服务器名等，www.baidu.com[:80]，端口可选
   * @param family 协议族，AF_INET、AF_INET6、AF_UNIX
   * @param sock_type socket类型，SOCK_STREAM、SOCK_DGRAM、SOCK_SEQPACKET
   * @param protocol 协议，IPPROTO_TCP、IPPROTO_UDP、IPPROTO_SCTP
   * @return std::vector<Address::s_ptr>
   */
  static auto GetAllTypeAddrByHost(std::string_view host, int family = AF_INET, int sock_type = 0, int protocol = 0)
      -> std::vector<Address::s_ptr>;

  /**
   * @brief 根据host获取满足对应条件的任意一个地址
   */
  static auto GetAnyOneAddrByHost(std::string_view host, int family = AF_INET, int sock_type = 0, int protocol = 0)
      -> s_ptr;

  /**
   * @brief 根据host获取满足条件的任意一个IP地址，因为一个域名可能对应多个IP地址
   */
  static auto GetAnyOneIPByHost(std::string_view host, int family = AF_INET, int sock_type = 0, int protocol = 0)
      -> std::shared_ptr<IPAddress>;

  /**
   * @brief 返回本机所有网卡的指定协议族的<网卡名：<地址、子网掩码位数>>
   */
  static auto GetAllInterfaceAddrInfo(int family = AF_INET)
      -> std::multimap<std::string, std::pair<Address::s_ptr, uint32_t>>;

  /**
   * @brief 返回本机指定名称网卡的指定协议族的<地址、子网掩码位数>
   */
  static auto GetInterfaceAddrInfo(std::string_view interface_name, int family = AF_INET)
      -> std::vector<std::pair<Address::s_ptr, uint32_t>>;

  /**
   * @brief 返回协议族
   */
  auto GetFamily() const -> int;

  /**
   * @brief 返回sockaddr指针，const重载
   */
  virtual auto GetSockAddr() const -> const sockaddr * = 0;

  /**
   * @brief 返回sockaddr指针，非const重载
   */
  virtual auto GetSockAddr() -> sockaddr * = 0;

  /**
   * @brief 返回sockaddr长度
   */
  virtual auto GetSockAddrLen() const -> socklen_t = 0;

  /**
   * @brief 返回地址字符串
   */
  auto ToString() const -> std::string;

  /**
   * @brief 将地址信息写入流
   */
  virtual auto DumpToStream(std::ostream &os) const -> std::ostream & = 0;

  auto operator<(const Address &rhs) const -> bool;

  auto operator==(const Address &rhs) const -> bool;

  auto operator!=(const Address &rhs) const -> bool;
};

class IPAddress : public Address {
 public:
  using s_ptr = std::shared_ptr<IPAddress>;

  /**
   * @brief 根据ip字符串和端口号创建IPAddress指针
   */
  static auto CreateAddr(std::string_view ip, uint16_t port = 0) -> s_ptr;

  /**
   * @brief 获取this对应的广播IP
   */
  virtual auto BroadCastAddress(uint32_t prefix_len) -> s_ptr = 0;

  /**
   * @brief 获取this对应的网络地址
   */
  virtual auto NetworkAddress(uint32_t prefix_len) -> s_ptr = 0;

  /**
   * @brief 获取this对应的子网掩码
   */
  virtual auto SubnetMask(uint32_t prefix_len) -> s_ptr = 0;

  /**
   * @brief 获取this的端口
   */
  virtual auto GetPort() const -> uint16_t = 0;

  /**
   * @brief 设置this的端口
   */
  virtual void SetPort(uint16_t port) = 0;
};

class IPv4Address : public IPAddress {
 public:
  using s_ptr = std::shared_ptr<IPv4Address>;

  /**
   * @brief 根据IP字符串（点分十进制）和端口号创建IPv4Address指针
   */
  static auto CreateAddr(std::string_view ip, uint16_t port) -> s_ptr;

  // IPv4Address();

  /**
   * @brief 构造函数，封装sockaddr_in
   */
  explicit IPv4Address(const sockaddr_in &addr);

  /**
   * @brief 通过二进制IP地址和端口初始化封装的sockaddr_in
   */
  explicit IPv4Address(uint32_t ip = INADDR_ANY, uint16_t port = 0);

  auto GetSockAddr() const -> const sockaddr * override;

  auto GetSockAddr() -> sockaddr * override;

  auto GetSockAddrLen() const -> socklen_t override;

  auto DumpToStream(std::ostream &os) const -> std::ostream & override;

  auto BroadCastAddress(uint32_t prefix_len) -> IPAddress::s_ptr override;

  auto NetworkAddress(uint32_t prefix_len) -> IPAddress::s_ptr override;

  auto SubnetMask(uint32_t prefix_len) -> IPAddress::s_ptr override;

  auto GetPort() const -> uint16_t override;

  void SetPort(uint16_t port) override;

 private:
  sockaddr_in addr_{};
};

class IPv6Address : public IPAddress {
 public:
  using s_ptr = std::shared_ptr<IPv6Address>;

  /**
   * @brief 根据IP字符串和端口号创建IPv6指针
   */
  static auto CreateAddr(std::string_view ip, uint16_t port) -> s_ptr;

  IPv6Address();

  /**
   * @brief 构造函数，封装sockaddr_in6
   */
  explicit IPv6Address(const sockaddr_in6 &addr);

  /**
   * @brief 通过二进制IP地址和端口初始化封装的sockaddr_in6
   */
  explicit IPv6Address(const uint8_t ip[16], uint16_t port);

  auto GetSockAddr() const -> const sockaddr * override;

  auto GetSockAddr() -> sockaddr * override;

  auto GetSockAddrLen() const -> socklen_t override;

  auto DumpToStream(std::ostream &os) const -> std::ostream & override;

  auto BroadCastAddress(uint32_t prefix_len) -> IPAddress::s_ptr override;

  auto NetworkAddress(uint32_t prefix_len) -> IPAddress::s_ptr override;

  auto SubnetMask(uint32_t prefix_len) -> IPAddress::s_ptr override;

  auto GetPort() const -> uint16_t override;

  void SetPort(uint16_t port) override;

 private:
  sockaddr_in6 addr_{};
};

class UnixAddress : public Address {
 public:
  using s_ptr = std::shared_ptr<UnixAddress>;

  UnixAddress();

  /**
   * @brief 构造函数，通过path构造UnixAddress
   */
  explicit UnixAddress(std::string_view path);

  void SetAddrlen(uint32_t);

  auto GetPath() const -> std::string;

  auto GetSockAddr() const -> const sockaddr * override;

  auto GetSockAddr() -> sockaddr * override;

  auto GetSockAddrLen() const -> socklen_t override;

  auto DumpToStream(std::ostream &os) const -> std::ostream & override;

 private:
  sockaddr_un addr_{};
  socklen_t addr_len_{};
};

class UnknownAddress : public Address {
 public:
  using s_ptr = std::shared_ptr<UnknownAddress>;
  explicit UnknownAddress(int family);
  explicit UnknownAddress(const sockaddr &addr);
  auto GetSockAddr() const -> const sockaddr * override;
  auto GetSockAddr() -> sockaddr * override;
  auto GetSockAddrLen() const -> socklen_t override;
  auto DumpToStream(std::ostream &os) const -> std::ostream & override;

 private:
  sockaddr addr_{};
};

/**
 * @brief 将Address指针写入流
 */
auto operator<<(std::ostream &os, const Address &addr) -> std::ostream &;

}  // namespace wtsclwq
#endif  // _ADDRESS_H_