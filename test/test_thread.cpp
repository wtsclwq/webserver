#include "server/config.h"
#include "server/env.h"
#include "server/log.h"
#include "server/server.h"
#include "server/thread.h"
#include "server/utils.h"

auto root_logger = wtsclwq::LoggerMgr::GetInstance() -> GetRoot();

int count1 = 0;
int count2 = 0;
std::mutex mtx;

void FuncNoLock(void *arg) {
  LOG_INFO(root_logger) << "name: " << wtsclwq::Thread::GetCurrName() << "\n"
                        << "this.name: " << wtsclwq::Thread::GetCurrPtr()->GetName() << "\n"
                        << "sys pthread name: " << wtsclwq::GetCurrSysThreadName() << "\n"
                        << "id: " << wtsclwq::GetCurrSysThreadId() << "\n"
                        << "this.id: " << wtsclwq::Thread::GetCurrPtr()->GetId() << "\n";
  LOG_INFO(root_logger) << "arg: " << *static_cast<int *>(arg);
  for (int i = 0; i < 1000000; ++i) {
    ++count1;
  }
}

void FuncLock(void *arg) {
  LOG_INFO(root_logger) << "name: " << wtsclwq::Thread::GetCurrName() << "\n"
                        << "this.name: " << wtsclwq::Thread::GetCurrPtr()->GetName() << "\n"
                        << "sys pthread name: " << wtsclwq::GetCurrSysThreadName() << "\n"
                        << "id: " << wtsclwq::GetCurrSysThreadId() << "\n"
                        << "this.id: " << wtsclwq::Thread::GetCurrPtr()->GetId() << "\n";
  LOG_INFO(root_logger) << "arg: " << *static_cast<int *>(arg);
  for (int i = 0; i < 1000000; ++i) {
    std::lock_guard<std::mutex> lock(mtx);
    ++count2;
  }
}

auto main(int argc, char **argv) -> int {
  wtsclwq::EnvMgr::GetInstance()->Init(argc, argv);
  wtsclwq::ConfigMgr::GetInstance()->LoadFromConfDir(wtsclwq::EnvMgr::GetInstance()->GetConfigPath());

  std::vector<wtsclwq::Thread::s_ptr> threads1;
  std::vector<wtsclwq::Thread::s_ptr> threads2;
  int arg = 123456;
  threads1.reserve(3);
  threads2.reserve(3);

  for (int i = 0; i < 3; ++i) {
    threads1.emplace_back(std::make_shared<wtsclwq::Thread>([capture0 = &arg] { return FuncNoLock(capture0); },
                                                            "NoLockThread" + std::to_string(i)));
    threads2.emplace_back(std::make_shared<wtsclwq::Thread>([capture0 = &arg] { return FuncLock(capture0); },
                                                            "LockedThread" + std::to_string(i)));
  }
  for (int i = 0; i < 3; ++i) {
    threads1[i]->Join();
    threads2[i]->Join();
  }

  LOG_INFO(root_logger) << "count1: " << count1;
  LOG_INFO(root_logger) << "count2: " << count2;
}