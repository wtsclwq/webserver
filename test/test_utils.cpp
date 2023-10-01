#include "server/env.h"
#include "server/log.h"
#include "server/server.h"
#include "server/utils.h"

auto root_logger = wtsclwq::LoggerMgr::GetInstance() -> GetRoot();

void Test2() { std::cout << wtsclwq::BacktraceToString() << std::endl; }
void Test1() { Test2(); }

void TestBacktrace() { Test1(); }

auto main() -> int {
  LOG_INFO(root_logger) << wtsclwq::GetCurrMs();
  LOG_INFO(root_logger) << wtsclwq::GetCurrUs();
  LOG_INFO(root_logger) << wtsclwq::GetCurrSysThreadId();
  LOG_INFO(root_logger) << wtsclwq::GetCurrSysThreadName();
  LOG_INFO(root_logger) << wtsclwq::ToUpper("hello world");
  LOG_INFO(root_logger) << wtsclwq::ToLower("HELLO WORLD");
  LOG_INFO(root_logger) << wtsclwq::StringUtil::Trim("  hello world  ");
  LOG_INFO(root_logger) << wtsclwq::StringUtil::TrimLeft("  hello world  ");
  LOG_INFO(root_logger) << wtsclwq::StringUtil::TrimRight("  hello world  ");
  LOG_INFO(root_logger) << wtsclwq::StrToTime("1977-01-11 00:00:00");
  LOG_INFO(root_logger) << wtsclwq::TimeToStr(0);

  std::vector<std::string> files;
  wtsclwq::FSUtil::ListAllFile(&files, "/workspaces/codespaces-blank/server", ".cpp");
  for (auto &file : files) {
    LOG_INFO(root_logger) << file;
  }

  TestBacktrace();
  return 0;
}