#include "address.h"
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <ostream>
#include <string_view>
#include <vector>
#include "endian.h"
#include "log.h"
#include "server/endian.h"
#include "server/log.h"

namespace wtsclwq {
static auto sys_logger = NAMED_LOGGER("system");

template <typename T>
static auto CreateMask(uint32_t bits) -> T {
  return (1 << (sizeof(T) * 8 - bits)) - 1;
}

template <typename T>
static auto CountBits(T value) -> uint32_t {
  uint32_t count = 0;
  while (value) {
    value &= value - 1;
    ++count;
  }
  return count;
}

Address::~Address() = default;

auto Address::CreateAddr(const sockaddr *addr, socklen_t addr_len) -> Address::s_ptr {
  if (addr == nullptr) {
    return nullptr;
  }

  Address::s_ptr res{};
  switch (addr->sa_family) {
    case AF_INET:
      res = std::make_shared<IPv4Address>(*reinterpret_cast<const sockaddr_in *>(addr));
      break;
    case AF_INET6:
      res = std::make_shared<IPv6Address>(*reinterpret_cast<const sockaddr_in6 *>(addr));
      break;
    default:
      res = std::make_shared<UnknownAddress>(*addr);
  }
  return res;
}

auto Address::GetAllTypeAddrByHost(std::string_view host, int family, int sock_type, int protocol)
    -> std::vector<Address::s_ptr> {
  std::vector<Address::s_ptr> res{};

  addrinfo hints{};
  // 没有设置的会自动初始化为0或nullptr
  hints.ai_family = family;
  hints.ai_socktype = sock_type;
  hints.ai_protocol = protocol;

  std::string node;
  const char *service = nullptr;

  // 检查 ipv6address serivce
  if (!host.empty() && host[0] == '[') {
    const char *endipv6 = static_cast<const char *>(memchr(host.data() + 1, ']', host.size() - 1));
    if (endipv6 != nullptr) {
      if (*(endipv6 + 1) == ':') {
        service = endipv6 + 2;
      }
      node = host.substr(1, endipv6 - host.data() - 1);
    }
  }

  // 检查 node serivce
  if (node.empty()) {
    service = static_cast<const char *>(memchr(host.data(), ':', host.size()));
    if (service != nullptr) {
      if (memchr(service + 1, ':', host.data() + host.size() - service - 1) == nullptr) {
        node = host.substr(0, service - host.data());
        ++service;
      }
    }
  }
  addrinfo *addr_list{};
  if (node.empty()) {
    node = host;
  }
  int ret = getaddrinfo(node.data(), service, &hints, &addr_list);
  if (ret != 0) {
    LOG_ERROR(sys_logger) << "getaddrinfo error: " << gai_strerror(ret);
    return res;
  }

  addrinfo *cur = addr_list;
  while (cur != nullptr) {
    res.push_back(CreateAddr(cur->ai_addr, cur->ai_addrlen));
    cur = cur->ai_next;
  }

  freeaddrinfo(addr_list);
  return res;
}

auto Address::GetAnyOneAddrByHost(std::string_view host, int family, int sock_type, int protocol) -> Address::s_ptr {
  std::vector<Address::s_ptr> res = GetAllTypeAddrByHost(host, family, sock_type, protocol);
  if (res.empty()) {
    return nullptr;
  }
  return res[0];
}

auto Address::GetAnyOneIPByHost(std::string_view host, int family, int sock_type, int protocol)
    -> std::shared_ptr<IPAddress> {
  std::vector<Address::s_ptr> res = GetAllTypeAddrByHost(host, family, sock_type, protocol);
  if (res.empty()) {
    return nullptr;
  }
  for (auto &addr : res) {
    auto ip_addr = std::dynamic_pointer_cast<IPAddress>(addr);
    if (ip_addr != nullptr) {
      return ip_addr;
    }
  }
  return nullptr;
}

auto Address::GetAllInterfaceAddrInfo(int family) -> std::multimap<std::string, std::pair<Address::s_ptr, uint32_t>> {
  std::multimap<std::string, std::pair<Address::s_ptr, uint32_t>> res{};

  ifaddrs *ifaddr_list{};
  if (getifaddrs(&ifaddr_list) != 0) {
    LOG_ERROR(sys_logger) << "getifaddrs error: " << strerror(errno);
    return res;
  }

  for (ifaddrs *cur = ifaddr_list; cur != nullptr; cur = cur->ifa_next) {
    if (cur->ifa_addr == nullptr || (family != AF_UNSPEC && family != cur->ifa_addr->sa_family)) {
      continue;
    }
    Address::s_ptr addr = nullptr;
    uint32_t prefix_len = UINT32_MAX;
    switch (cur->ifa_addr->sa_family) {
      case AF_INET: {
        addr = std::make_shared<IPv4Address>(*reinterpret_cast<sockaddr_in *>(cur->ifa_addr));
        uint32_t netmask = (reinterpret_cast<sockaddr_in *>(cur->ifa_netmask))->sin_addr.s_addr;
        prefix_len = CountBits(netmask);
        break;
      }
      case AF_INET6: {
        addr = std::make_shared<IPv6Address>(*reinterpret_cast<sockaddr_in6 *>(cur->ifa_addr));
        in6_addr netmask = (reinterpret_cast<sockaddr_in6 *>(cur->ifa_netmask))->sin6_addr;
        prefix_len = 0;
        for (int i = 0; i < 16; ++i) {
          prefix_len += CountBits(netmask.s6_addr[i]);
        }
        break;
      }
      default:
        addr = std::make_shared<UnknownAddress>(*(cur->ifa_addr));
    }
    res.emplace(cur->ifa_name, std::make_pair(addr, prefix_len));
  }
  freeifaddrs(ifaddr_list);
  return res;
}

auto Address::GetInterfaceAddrInfo(std::string_view interface_name, int family)
    -> std::vector<std::pair<Address::s_ptr, uint32_t>> {
  std::vector<std::pair<Address::s_ptr, uint32_t>> res{};

  if (interface_name.empty() || interface_name == "*") {
    if (family == AF_INET || family == AF_UNSPEC) {
      res.emplace_back(std::make_shared<IPv4Address>(), 0);
    }
    if (family == AF_INET6 || family == AF_UNSPEC) {
      res.emplace_back(std::make_shared<IPv6Address>(), 0);
    }
    return res;
  }
  auto all_addr_info = GetAllInterfaceAddrInfo(family);
  auto range = all_addr_info.equal_range(interface_name.data());
  for (auto it = range.first; it != range.second; ++it) {
    res.push_back(it->second);
  }
  return res;
}

auto Address::GetFamily() const -> int { return GetSockAddr()->sa_family; }

auto Address::ToString() const -> std::string {
  std::stringstream ss;
  DumpToStream(ss);
  return ss.str();
}

auto Address::operator<(const Address &rhs) const -> bool {
  socklen_t min_len = std::min(GetSockAddrLen(), rhs.GetSockAddrLen());
  int res = memcmp(GetSockAddr(), rhs.GetSockAddr(), min_len);
  if (res < 0) {
    return true;
  }
  if (res > 0) {
    return false;
  }
  return GetSockAddrLen() < rhs.GetSockAddrLen();
}

auto Address::operator==(const Address &rhs) const -> bool {
  return GetSockAddrLen() == rhs.GetSockAddrLen() && memcmp(GetSockAddr(), rhs.GetSockAddr(), GetSockAddrLen()) == 0;
}

auto Address::operator!=(const Address &rhs) const -> bool { return !(*this == rhs); }

auto IPAddress::CreateAddr(std::string_view ip, uint16_t port) -> IPAddress::s_ptr {
  if (ip.empty()) {
    return nullptr;
  }
  addrinfo hints{};
  addrinfo *addr_list{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_flags = AI_NUMERICHOST;

  int ret = getaddrinfo(ip.data(), nullptr, &hints, &addr_list);
  if (ret != 0) {
    LOG_ERROR(sys_logger) << "getaddrinfo error: " << gai_strerror(ret);
    return nullptr;
  }
  auto res = std::dynamic_pointer_cast<IPAddress>(Address::CreateAddr(addr_list->ai_addr, addr_list->ai_addrlen));
  if (res != nullptr) {
    res->SetPort(port);
  }
  freeaddrinfo(addr_list);
  return res;
}

auto IPv4Address::CreateAddr(std::string_view ip, uint16_t port) -> IPv4Address::s_ptr {
  if (ip.empty()) {
    return nullptr;
  }

  auto res = std::make_shared<IPv4Address>();
  res->addr_.sin_port = OnlyByteswapOnLittleEndian(port);
  int ret = inet_pton(AF_INET, ip.data(), &res->addr_.sin_addr);
  if (ret <= 0) {
    LOG_ERROR(sys_logger) << "inet_pton error: " << strerror(errno);
    return nullptr;
  }
  return res;
}

// IPv4Address::IPv4Address() = default;

IPv4Address::IPv4Address(const sockaddr_in &addr) : addr_(addr) {}

IPv4Address::IPv4Address(uint32_t ip, uint16_t port) {
  addr_.sin_family = AF_INET;
  addr_.sin_addr.s_addr = OnlyByteswapOnLittleEndian(ip);
  addr_.sin_port = OnlyByteswapOnLittleEndian(port);
}

auto IPv4Address::GetSockAddr() const -> const sockaddr * { return reinterpret_cast<const sockaddr *>(&addr_); }

auto IPv4Address::GetSockAddr() -> sockaddr * { return reinterpret_cast<sockaddr *>(&addr_); }

auto IPv4Address::GetSockAddrLen() const -> socklen_t { return sizeof(addr_); }

auto IPv4Address::SetPort(uint16_t port) -> void { addr_.sin_port = OnlyByteswapOnLittleEndian(port); }

auto IPv4Address::GetPort() const -> uint16_t { return OnlyByteswapOnLittleEndian(addr_.sin_port); }

auto IPv4Address::DumpToStream(std::ostream &os) const -> std::ostream & {
  uint32_t addr = OnlyByteswapOnLittleEndian(addr_.sin_addr.s_addr);
  os << (addr >> 24 & 0xff) << "." << (addr >> 16 & 0xff) << "." << (addr >> 8 & 0xff) << "." << (addr & 0xff);
  os << ":" << GetPort();
  return os;
}

auto IPv4Address::BroadCastAddress(uint32_t prefix_len) -> IPAddress::s_ptr {
  if (prefix_len > 32) {
    return nullptr;
  }
  sockaddr_in broadcast_addr = addr_;
  broadcast_addr.sin_addr.s_addr |= OnlyByteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));
  return std::make_shared<IPv4Address>(broadcast_addr);
}

auto IPv4Address::NetworkAddress(uint32_t prefix_len) -> IPAddress::s_ptr {
  if (prefix_len > 32) {
    return nullptr;
  }
  sockaddr_in net_addr = addr_;
  net_addr.sin_addr.s_addr &= OnlyByteswapOnLittleEndian(~CreateMask<uint32_t>(prefix_len));
  return std::make_shared<IPv4Address>(net_addr);
}

auto IPv4Address::SubnetMask(uint32_t prefix_len) -> IPAddress::s_ptr {
  if (prefix_len > 32) {
    return nullptr;
  }
  sockaddr_in mask_addr{};
  mask_addr.sin_family = AF_INET;
  mask_addr.sin_addr.s_addr = ~OnlyByteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));
  return std::make_shared<IPv4Address>(mask_addr);
}

auto IPv6Address::CreateAddr(std::string_view ip, uint16_t port) -> IPv6Address::s_ptr {
  if (ip.empty()) {
    return nullptr;
  }

  auto res = std::make_shared<IPv6Address>();
  res->addr_.sin6_port = OnlyByteswapOnLittleEndian(port);
  int ret = inet_pton(AF_INET6, ip.data(), &res->addr_.sin6_addr);
  if (ret <= 0) {
    LOG_ERROR(sys_logger) << "inet_pton error: " << strerror(errno);
    return nullptr;
  }
  return res;
}

IPv6Address::IPv6Address() {
  addr_.sin6_family = AF_INET6;
  addr_.sin6_port = 0;
  addr_.sin6_flowinfo = 0;
  addr_.sin6_scope_id = 0;
}

IPv6Address::IPv6Address(const sockaddr_in6 &addr) : addr_(addr) {}

auto IPv6Address::GetSockAddr() const -> const sockaddr * { return reinterpret_cast<const sockaddr *>(&addr_); }

auto IPv6Address::GetSockAddr() -> sockaddr * { return reinterpret_cast<sockaddr *>(&addr_); }

auto IPv6Address::GetSockAddrLen() const -> socklen_t { return sizeof(addr_); }

auto IPv6Address::SetPort(uint16_t port) -> void { addr_.sin6_port = OnlyByteswapOnLittleEndian(port); }

auto IPv6Address::GetPort() const -> uint16_t { return OnlyByteswapOnLittleEndian(addr_.sin6_port); }

auto IPv6Address::DumpToStream(std::ostream &os) const -> std::ostream & {
  os << "[";
  auto *addr = reinterpret_cast<const uint8_t *>(addr_.sin6_addr.s6_addr);
  bool used_zeros = false;
  for (size_t i = 0; i < 8; ++i) {
    if (addr[i] == 0 && !used_zeros) {
      continue;
    }
    if ((i != 0) && addr[i - 1] == 0 && !used_zeros) {
      os << ":";
      used_zeros = true;
    }
    if (i != 0) {
      os << ":";
    }
    os << std::hex << static_cast<int>(addr[i]) << std::dec;
  }

  if (!used_zeros && addr[7] == 0) {
    os << "::";
  }

  os << "]:" << OnlyByteswapOnLittleEndian(addr_.sin6_port);
  return os;
}

auto IPv6Address::BroadCastAddress(uint32_t prefix_len) -> IPAddress::s_ptr {
  if (prefix_len > 128) {
    return nullptr;
  }
  sockaddr_in6 broadcast_addr = addr_;
  broadcast_addr.sin6_addr.s6_addr[prefix_len / 8] |= CreateMask<uint8_t>(prefix_len % 8);
  for (size_t i = prefix_len / 8 + 1; i < 16; ++i) {
    broadcast_addr.sin6_addr.s6_addr[i] = 0xff;
  }
  return std::make_shared<IPv6Address>(broadcast_addr);
}

auto IPv6Address::NetworkAddress(uint32_t prefix_len) -> IPAddress::s_ptr {
  if (prefix_len > 128) {
    return nullptr;
  }
  sockaddr_in6 net_addr = addr_;
  net_addr.sin6_addr.s6_addr[prefix_len / 8] &= CreateMask<uint8_t>(prefix_len % 8);
  for (size_t i = prefix_len / 8 + 1; i < 16; ++i) {
    net_addr.sin6_addr.s6_addr[i] = 0;
  }
  return std::make_shared<IPv6Address>(net_addr);
}

auto IPv6Address::SubnetMask(uint32_t prefix_len) -> IPAddress::s_ptr {
  if (prefix_len > 128) {
    return nullptr;
  }
  sockaddr_in6 mask_addr{};
  mask_addr.sin6_family = AF_INET6;
  mask_addr.sin6_addr.s6_addr[prefix_len / 8] = ~CreateMask<uint8_t>(prefix_len % 8);
  for (size_t i = prefix_len / 8 + 1; i < 16; ++i) {
    mask_addr.sin6_addr.s6_addr[i] = 0xff;
  }
  return std::make_shared<IPv6Address>(mask_addr);
}

UnixAddress::UnixAddress() {
  const size_t max_addr_len = sizeof(reinterpret_cast<sockaddr_un *>(0)->sun_path) - 1;
  addr_.sun_family = AF_UNIX;
  addr_len_ = offsetof(sockaddr_un, sun_path) + max_addr_len;
}

UnixAddress::UnixAddress(std::string_view path) : UnixAddress() {
  size_t len = std::min(path.size(), static_cast<size_t>(sizeof(addr_.sun_path) - 1));
  memcpy(addr_.sun_path, path.data(), len);
  addr_len_ = offsetof(sockaddr_un, sun_path) + len;
  addr_.sun_path[len] = '\0';
}

auto UnixAddress::SetAddrlen(uint32_t len) -> void { addr_len_ = len; }

auto UnixAddress::GetPath() const -> std::string {
  std::stringstream ss;
  if (addr_len_ > offsetof(sockaddr_un, sun_path) && addr_.sun_path[0] == '\0') {
    ss << "\\0" << std::string(addr_.sun_path + 1, addr_len_ - offsetof(sockaddr_un, sun_path) - 1);
  } else {
    ss << addr_.sun_path;
  }
  return ss.str();
}

auto UnixAddress::GetSockAddr() const -> const sockaddr * { return reinterpret_cast<const sockaddr *>(&addr_); }

auto UnixAddress::GetSockAddr() -> sockaddr * { return reinterpret_cast<sockaddr *>(&addr_); }

auto UnixAddress::GetSockAddrLen() const -> socklen_t { return addr_len_; }

auto UnixAddress::DumpToStream(std::ostream &os) const -> std::ostream & {
  os << "unix:";
  if (addr_len_ > offsetof(sockaddr_un, sun_path) && addr_.sun_path[0] == '\0') {
    os << "\\0" << std::string_view(addr_.sun_path + 1, addr_len_ - offsetof(sockaddr_un, sun_path) - 1);
  } else {
    os << addr_.sun_path;
  }
  return os;
}

UnknownAddress::UnknownAddress(int family) { addr_.sa_family = family; }

UnknownAddress::UnknownAddress(const sockaddr &addr) { addr_ = addr; }

auto UnknownAddress::GetSockAddr() const -> const sockaddr * { return &addr_; }

auto UnknownAddress::GetSockAddr() -> sockaddr * { return &addr_; }

auto UnknownAddress::GetSockAddrLen() const -> socklen_t { return sizeof(addr_); }

auto UnknownAddress::DumpToStream(std::ostream &os) const -> std::ostream & {
  os << "unknow: sa_family=" << addr_.sa_family << ", addr_len=" << GetSockAddrLen();
  return os;
}

auto operator<<(std::ostream &os, const Address &addr) -> std::ostream & {
  addr.DumpToStream(os);
  return os;
}
}  // namespace wtsclwq