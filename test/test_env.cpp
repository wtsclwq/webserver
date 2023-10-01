#include "server/env.h"
#include "server/log.h"

auto logger = ROOT_LOGGER;

auto env = wtsclwq::EnvMgr::GetInstance();

auto main(int argc, char **argv) -> int {
  env->AddHelp("h", "print help message");

  bool is_print_help = false;
  if (env->Init(argc, argv)) {
    is_print_help = true;
  }
  if (is_print_help) {
    env->PrintHelps();
  }

  LOG_INFO(logger) << "exe: " << env->GetExeAbsPath();
  LOG_INFO(logger) << "program name: " << env->GetProgramName();
  LOG_INFO(logger) << "pwd: " << env->GetPwd();
  LOG_INFO(logger) << "conf path: " << env->GetConfigPath();

  env->AddArg("key1", "value1");
  env->AddArg("key2", "value2");
  LOG_INFO(logger) << "key1: " << env->GetArg("key1");
  LOG_INFO(logger) << "key2: " << env->GetArg("key2");

  LOG_INFO(logger) << "key3: " << env->GetArg("key3");
  return 0;
}