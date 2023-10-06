#include "address.h"
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string_view>
#include <vector>
#include "log.h"
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
  res->addr_.sin_port = 
  }
}  // namespace wtsclwq