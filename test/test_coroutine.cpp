#include <memory>

#include "server/coroutine.h"
#include "server/log.h"
#include "server/server.h"
#include "server/utils.h"

auto root_logger = ROOT_LOGGER;

void RuInCoroutine2() {
  LOG_INFO(root_logger) << "RunInCoroutine2 start";
  LOG_INFO(root_logger) << "RunInCoroutine2 end";
}

void RunInCoroutine1() {
  LOG_INFO(root_logger) << "RunInCoroutine1 start";

  LOG_INFO(root_logger) << "RunInCoroutine1 before yield";
  wtsclwq::Coroutine::GetThreadRunningCoroutine()->Yield();
  LOG_INFO(root_logger) << "RunInCoroutine1 after yield";

  LOG_INFO(root_logger) << "RunInCoroutine1 end";
}

void TestCoroutine() {
  LOG_INFO(root_logger) << "TestCoroutine start";

  wtsclwq::Coroutine::InitThreadToCoMod();

  auto co1 = std::make_shared<wtsclwq::Coroutine>(RunInCoroutine1, 1024 * 1024, true,
                                                  wtsclwq::Coroutine::GetThreadRunningCoroutine());

  LOG_INFO(root_logger) << "xxxxx main use count: " << wtsclwq::Coroutine::GetThreadRunningCoroutine().use_count();

  LOG_INFO(root_logger) << "use count" << co1.use_count();

  LOG_INFO(root_logger) << "TestCoroutine before resume";

  co1->Resume();

  LOG_INFO(root_logger) << "TestCoroutine after resume";

  LOG_INFO(root_logger) << "xxxxx main use count: " << wtsclwq::Coroutine::GetThreadRunningCoroutine().use_count();

  LOG_INFO(root_logger) << "use count: " << co1.use_count();

  LOG_INFO(root_logger) << "State: " << static_cast<int>(co1->GetState());

  LOG_INFO(root_logger) << "before resume again";

  co1->Resume();

  LOG_INFO(root_logger) << "after resume again";

  LOG_INFO(root_logger) << "xxxxx main use count: " << wtsclwq::Coroutine::GetThreadRunningCoroutine().use_count();

  LOG_INFO(root_logger) << "use count: " << co1.use_count();

  LOG_INFO(root_logger) << "State: " << static_cast<int>(co1->GetState());

  co1->ResetTaskFunc(RuInCoroutine2);

  co1->Resume();
  
  LOG_INFO(root_logger) << "xxxxx main use count: " << wtsclwq::Coroutine::GetThreadRunningCoroutine().use_count();

  LOG_INFO(root_logger) << "use count: " << co1.use_count();

  LOG_INFO(root_logger) << "TestCoroutine end";

  LOG_INFO(root_logger) << "xxxxx main use count: " << wtsclwq::Coroutine::GetThreadRunningCoroutine().use_count();
}

auto main(int argc, char **argv) -> int {
  wtsclwq::EnvMgr::GetInstance()->Init(argc, argv);
  wtsclwq::ConfigMgr::GetInstance()->LoadFromConfDir(wtsclwq::EnvMgr::GetInstance()->GetConfigPath());

  wtsclwq::SetCurrSysThreadName("main_thread");
  LOG_INFO(root_logger) << "main begin";

  std::vector<wtsclwq::Thread::s_ptr> thrs;
  thrs.reserve(2);
  for (int i = 0; i < 1; i++) {
    thrs.push_back(std::make_shared<wtsclwq::Thread>(&TestCoroutine, "thread_" + std::to_string(i)));
  }

  for (const auto &i : thrs) {
    i->Join();
  }

  LOG_INFO(root_logger) << "main end";
  return 0;
}