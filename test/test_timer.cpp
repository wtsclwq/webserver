#include "server/log.h"
#include "server/server.h"
#include "server/sock_io_scheduler.h"

auto root_logger = ROOT_LOGGER;

int timeout = 1000;
wtsclwq::Timer::s_ptr timer;

void TimerCallback() {
  LOG_INFO(root_logger) << "TimerCallback"
                        << "timeout = " << timeout;
  timeout += 1000;
  if (timeout < 5000) {
    timer->Reset(timeout, true);
  } else {
    timer->Cancel();
  }
}

void TestTimer() {
}

auto main() -> int {
  wtsclwq::SockIoScheduler::s_ptr scheduler = std::make_shared<wtsclwq::SockIoScheduler>();
  scheduler->Start();
  timer = scheduler->AddTimer(1000, TimerCallback, true);
  scheduler->AddTimer(500, [] { LOG_INFO(root_logger) << "500ms"; });
  scheduler->AddTimer(5000, [] { LOG_INFO(root_logger) << "5000ms"; });
  scheduler->Stop();
  return 0;
}