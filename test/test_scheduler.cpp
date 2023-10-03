#include <memory>
#include "server/coroutine.h"
#include "server/log.h"
#include "server/scheduler.h"
#include "server/server.h"
#include "server/utils.h"

static auto root_logger = ROOT_LOGGER;

void TestCoroutine1() {
  LOG_INFO(root_logger) << "TestCoroutine1 start";

  LOG_INFO(root_logger) << "TestCoroutine1 add self into scheduler before";

  // 将当前协程加入到任务队列中
  wtsclwq::Scheduler::GetThreadScheduler()->Schedule(wtsclwq::Coroutine::GetThreadRunningCoroutine());

  LOG_INFO(root_logger) << "TestCoroutine1 add self into scheduler after";

  LOG_INFO(root_logger) << "TestCoroutine1 before yield";

  wtsclwq::Coroutine::GetThreadRunningCoroutine()->Yield();

  LOG_INFO(root_logger) << "TestCoroutine1 after yield";

  LOG_INFO(root_logger) << "TestCoroutine1 end";
}

void TestCoroutine2() {
  LOG_INFO(root_logger) << "TestCoroutine2 start";

  // 一个线程内同一时间只有一个协程在运行，线程调度协程的本质就是按顺序执行任务队列里的写成
  // 由于必须等待一个协程执行完毕或者Yield才能执行下一个协程，因此任意一个协程的阻塞都会影响整个线程的携程调度
  // 睡眠的3秒钟之内调度器不会调度新的协程，对sleep函数进行hook之后可以改变这种情况
  // 所以hook的本质其实将原本的阻塞操作改为了非阻塞操作，然后加入一些机制可以达到协作式调度的效果
  LOG_INFO(root_logger) << "TestCoroutine2 sleep before";
  sleep(3);
  LOG_INFO(root_logger) << "TestCoroutine2 sleep after";
  LOG_INFO(root_logger) << "TestCoroutine2 end";
}

void TestCoroutine3() {
  LOG_INFO(root_logger) << "TestCoroutine3 start";
  LOG_INFO(root_logger) << "TestCoroutine3 end";
}

void TestCoroutine4() {
  static int count = 0;
  LOG_INFO(root_logger) << "TestCoroutine4 start count = " << count;
  LOG_INFO(root_logger) << "TestCoroutine4 end count = " << count;
  count++;
}

void TestCoroutine5() {
  LOG_INFO(root_logger) << "TestCoroutine5 start";
  for (int i = 0; i < 3; i++) {
    wtsclwq::Scheduler::GetThreadScheduler()->Schedule([]() { TestCoroutine1(); }, wtsclwq::GetCurrSysThreadId());
  }
  LOG_INFO(root_logger) << "TestCoroutine5 end";
}

auto main() -> int {
  LOG_INFO(root_logger) << "main start";

  // 创建调度器，默认状态下使用创建者线程，相当于先用Schedule()攒下一波协程，然后用Stop()切换到调度器的run方法将这些协程
  // 消耗掉，然后再返回main函数往下执行
  wtsclwq::Scheduler sc{};

  sc.Schedule(&TestCoroutine1);
  sc.Schedule(&TestCoroutine2);

  auto co = std::make_shared<wtsclwq::Coroutine>(TestCoroutine3);
  sc.Schedule(co);

  // 创建调度线程，开始任务调度，如果只使用main函数线程进行调度，那start相当于什么也没做
  sc.Start();

  /**
   * 只要调度器未停止，就可以添加调度任务
   * 包括在子协程中也可以通过sylar::Scheduler::GetThis()->scheduler()的方式继续添加调度任务
   */
  sc.Schedule(&TestCoroutine4);

  // 通过Schedule()添加的任务会在调度器停止之前执行完毕
  sc.Stop();

  LOG_INFO(root_logger) << "main end";
  return 0;
}