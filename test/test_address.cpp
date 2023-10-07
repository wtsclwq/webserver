#include <sys/socket.h>
#include <memory>
#include <string_view>
#include "server/address.h"
#include "server/log.h"
#include "server/server.h"

auto root_logger = ROOT_LOGGER;

auto FamilyToStr(int family) -> const char * {
  switch (family) {
    case AF_INET:
      return "AF_INET";
    case AF_INET6:
      return "AF_INET6";
    case AF_UNIX:
      return "AF_UNIX";
    default:
      return "UNKNOWN";
  }
}

void TestIfaces(int family) {
  LOG_INFO(root_logger) << "TestIfaces: " << FamilyToStr(family) << " begin";

  auto res = wtsclwq::Address::GetAllInterfaceAddrInfo(family);
  if (res.empty()) {
    LOG_INFO(root_logger) << "No interface found";
    return;
  }
  for (auto &interface : res) {
    LOG_INFO(root_logger) << interface.first << " " << interface.second.first->ToString() << " "
                          << interface.second.second;
  }
  LOG_INFO(root_logger) << "TestIfaces: " << FamilyToStr(family) << " end";
}

void TestOnInterface(std::string_view iface, int famliy) {
  LOG_INFO(root_logger) << "TestOnInterface: " << iface << " " << FamilyToStr(famliy) << " begin";
  auto res = wtsclwq::Address::GetInterfaceAddrInfo(iface, famliy);
  if (res.empty()) {
    LOG_INFO(root_logger) << "No interface found";
    return;
  }
  for (auto &interface : res) {
    LOG_INFO(root_logger) << interface.first->ToString() << " " << interface.second;
  }
  LOG_INFO(root_logger) << "TestOnInterface: " << iface << " " << FamilyToStr(famliy) << " end";
}

void TestParse(std::string_view host) {
  LOG_INFO(root_logger) << "TestParse: " << host << " begin";

  LOG_INFO(root_logger) << "GetAllTypeAddrByHost: " << host;
  auto res = wtsclwq::Address::GetAllTypeAddrByHost(host, AF_INET);
  if (res.empty()) {
    LOG_INFO(root_logger) << "No interface found";
    return;
  }
  for (auto &addr : res) {
    LOG_INFO(root_logger) << addr->ToString();
  }

  LOG_INFO(root_logger) << "GetAnyOneAddrByHost: " << host;
  auto any = wtsclwq::Address::GetAnyOneAddrByHost(host, AF_INET);

  LOG_INFO(root_logger) << "GetAnyIPAddressByHost: " << host;
  auto ip = wtsclwq::Address::GetAnyOneIPByHost(host, AF_INET);

  LOG_INFO(root_logger) << "TestParse: " << host << " end";
}

void TestParse6(std::string_view host) {
  LOG_INFO(root_logger) << "TestParse: " << host << " begin";

  LOG_INFO(root_logger) << "GetAllTypeAddrByHost: " << host;
  auto res = wtsclwq::Address::GetAllTypeAddrByHost(host, AF_INET6);
  if (res.empty()) {
    LOG_INFO(root_logger) << "No interface found";
    return;
  }
  for (auto &addr : res) {
    LOG_INFO(root_logger) << addr->ToString();
  }

  LOG_INFO(root_logger) << "GetAnyOneAddrByHost: " << host;
  auto any = wtsclwq::Address::GetAnyOneAddrByHost(host, AF_INET6);

  LOG_INFO(root_logger) << "GetAnyIPAddressByHost: " << host;
  auto ip = wtsclwq::Address::GetAnyOneIPByHost(host, AF_INET6);

  LOG_INFO(root_logger) << "TestParse: " << host << " end";
}

void TestIpv4() {
  LOG_INFO(root_logger) << "TestIpv4 begin";

  auto addr = wtsclwq::IPv4Address::CreateAddr("127.0.0.1", 0);
  LOG_INFO(root_logger) << "addr: " << addr->ToString();
  LOG_INFO(root_logger) << "family: " << FamilyToStr(addr->GetFamily());
  LOG_INFO(root_logger) << "port: " << addr->GetPort();
  LOG_INFO(root_logger) << "addr len: " << addr->GetSockAddrLen();
  LOG_INFO(root_logger) << "broadcast: " << addr->BroadCastAddress(24)->ToString();
  LOG_INFO(root_logger) << "network: " << addr->NetworkAddress(24)->ToString();
  LOG_INFO(root_logger) << "subnet: " << addr->SubnetMask(24)->ToString();

  LOG_INFO(root_logger) << "TestIpv4 end";
}

void TestIpv6() {
  LOG_INFO(root_logger) << "TestIpv6 begin";

  auto addr = wtsclwq::IPv6Address::CreateAddr("::1", 0);
  LOG_INFO(root_logger) << "addr: " << addr->ToString();
  LOG_INFO(root_logger) << "family: " << FamilyToStr(addr->GetFamily());
  LOG_INFO(root_logger) << "port: " << addr->GetPort();
  LOG_INFO(root_logger) << "addr len: " << addr->GetSockAddrLen();
  LOG_INFO(root_logger) << "broadcast: " << addr->BroadCastAddress(24)->ToString();
  LOG_INFO(root_logger) << "network: " << addr->NetworkAddress(24)->ToString();
  LOG_INFO(root_logger) << "subnet: " << addr->SubnetMask(24)->ToString();

  LOG_INFO(root_logger) << "TestIpv6 end";
}

void TestUnix() {
  LOG_INFO(root_logger) << "TestUnix begin";

  auto addr = std::make_shared<wtsclwq::UnixAddress>("/tmp/test.sock");
  LOG_INFO(root_logger) << "addr: " << addr->ToString();
  LOG_INFO(root_logger) << "family: " << FamilyToStr(addr->GetFamily());
  LOG_INFO(root_logger) << "path: " << addr->GetPath();
  LOG_INFO(root_logger) << "addr len: " << addr->GetSockAddrLen();

  LOG_INFO(root_logger) << "TestUnix end";
}

auto main() -> int {
  TestIfaces(AF_INET);
  TestIfaces(AF_INET6);

  TestOnInterface("eth0", AF_INET);
  TestOnInterface("eth0", AF_INET6);

  TestParse("www.baidu.com");
  TestParse("www.google.com");
  TestParse("www.sina.com.cn");
  TestParse("127.0.0.1");
  TestParse6("[::]");
  TestParse("127.0.0.1:80");
  TestParse("127.0.0.1:http");
  TestParse("127.0.0.1:ftp");
  TestParse("localhost");
  TestParse("localhost:80");

  TestIpv4();

  TestIpv6();

  TestUnix();

  return 0;
}