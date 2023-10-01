#include "server/config.h"
#include "server/env.h"
#include "server/log.h"

auto root_logger = ROOT_LOGGER;
auto logger1 = NAMED_LOGGER("logger1");
auto logger2 = NAMED_LOGGER("logger2");

auto main(int argc, char **argv) -> int {
  std::cout << "===============before load=================\n";
  std::cout << "===============root logger=================\n";
  LOG_INFO(root_logger) << root_logger->FlushConfigToYmal() << "\n";
  std::cout << "===============logger1=================\n";
  LOG_INFO(logger1) << logger1->FlushConfigToYmal() << "\n";
  std::cout << "===============logger2=================\n";
  LOG_INFO(logger2) << logger2->FlushConfigToYmal() << "\n";

  wtsclwq::EnvMgr::GetInstance()->Init(argc, argv);
  wtsclwq::ConfigMgr::GetInstance()->LoadFromConfDir(wtsclwq::EnvMgr::GetInstance()->GetConfigPath());

  std::cout << "===============after load=================\n";
  std::cout << "===============root logger=================\n";
  LOG_INFO(root_logger) << root_logger->FlushConfigToYmal() << "\n";
  std::cout << "===============logger1=================\n";
  LOG_INFO(logger1) << logger1->FlushConfigToYmal() << "\n";
  std::cout << "===============logger2=================\n";
  LOG_INFO(logger2) << logger2->FlushConfigToYmal() << "\n"; // 配置中将logger2的level设置为warn，所以info不会输出

  LOG_FATAL(root_logger) << "root logger fatal";
  LOG_ERROR(root_logger) << "root logger error";
  LOG_WARN(root_logger) << "root logger warn";
  LOG_DEBUG(root_logger) << "root logger debug";
  LOG_INFO(root_logger) << "root logger info";

  LOG_FATAL(logger1) << "logger1 fatal";
  LOG_ERROR(logger1) << "logger1 error";
  LOG_WARN(logger1) << "logger1 warn";
  LOG_DEBUG(logger1) << "logger1 debug";
  LOG_INFO(logger1) << "logger1 info";

  LOG_FATAL(logger2) << "logger2 fatal";
  LOG_ERROR(logger2) << "logger2 error";
  LOG_WARN(logger2) << "logger2 warn";
  LOG_DEBUG(logger2) << "logger2 debug";
  LOG_INFO(logger2) << "logger2 info";
}