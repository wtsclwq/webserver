#include "scheduler.h"
#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>
#include "coroutine.h"
#include "log.h"
#include "macro.h"
#include "server/utils.h"

namespace wtsclwq {
static auto sys_logger = NAMED_LOGGER("system");

// 当前线程的调度器
static thread_local Scheduler::s_ptr thread_scheduler = nullptr;

// 当前线程的调度协程，对于线程池中的线程来说，调度协程==主协程， 对于creator线程来说，调度协程 != 主协程
static thread_local Coroutine::s_ptr thread_schedule_coroutine = nullptr;

Scheduler::Scheduler(size_t thread_num, bool use_creator, std::string_view name)
    : name_(name), use_creator_thread_(use_creator) {
  ASSERT(thread_num > 0);
  if (use_creator_thread_) {
    thread_num--;
    // 设置creator线程名称
    Thread::SetCurrName(name_);

    // 设置创建者线程内的调度器
    ASSERT(GetThreadScheduler() == nullptr);
    thread_scheduler = this->shared_from_this();

    // 将创建者线程设置为协程模式
    Coroutine::InitThreadToCoMod();
    creator_schedule_coroutine_ =
        std::make_shared<Coroutine>([this] { Run(); }, 0, true, Coroutine::GetThreadMainCoroutine());
    creator_thread_id_ = GetCurrSysThreadId();
    thread_ids_.emplace_back(creator_thread_id_);
  } else {
    creator_thread_id_ = -1;
  }

  thread_count_ = thread_num;
}

Scheduler::~Scheduler() {
  LOG_DEBUG(sys_logger) << "Scheduler " << name_ << " is destroyed";
  ASSERT(is_stoped_);
  if (GetThreadScheduler().get() == this) {
    thread_scheduler = nullptr;
  }
}

auto Scheduler::GetName() -> std::string { return name_; }

auto Scheduler::GetThreadScheduler() -> s_ptr { return thread_scheduler; }

void Scheduler::InitThreadScheduler() {
  ASSERT(thread_scheduler == nullptr);
  thread_scheduler = this->shared_from_this();
}

auto Scheduler::GetThreadScheduleCoroutine() -> Coroutine::s_ptr { return thread_schedule_coroutine; }

template <typename Scheduleable>
void Scheduler::Schedule(Scheduleable sa, int target_thread_id) {
  bool need_tickle = false;
  {
    std::lock_guard<MutexType> lock(mutex_);
    need_tickle = NonLockScheduleImpl(sa, target_thread_id);
  }
  if (need_tickle) {
    Tickle();
  }
}

template <typename Scheduleable>
auto Scheduler::NonLockScheduleImpl(Scheduleable sa, int target_thread_id) -> bool {
  bool need_tickle = task_queue_.empty();
  ScheduleTask task(sa, target_thread_id);
  if (task.coroutine_ != nullptr || task.func_ != nullptr) {
    task_queue_.emplace_back(std::move(task));
  }
  return need_tickle;
}

void Scheduler::Start() {
  LOG_DEBUG(sys_logger) << "Scheduler " << name_ << " is starting";
  std::lock_guard<MutexType> lock(mutex_);
  if (is_stoped_) {
    LOG_ERROR(sys_logger) << "Scheduler " << name_ << " is already stoped";
    return;
  }
  ASSERT(thread_pool_.empty());
  thread_pool_.reserve(thread_count_);
  for (size_t i = 0; i < thread_count_; i++) {
    auto thread = std::make_shared<Thread>([this] { Run(); }, name_ + "_" + std::to_string(i));
    thread_pool_.emplace_back(thread);
    thread_ids_.emplace_back(thread->GetId());
  }
}

auto Scheduler::IsStopable() -> bool {
  std::lock_guard<MutexType> lock(mutex_);
  return is_stoped_ && task_queue_.empty() && active_thread_count_ == 0;
}

void Scheduler::Tickle() { LOG_DEBUG(sys_logger) << "Scheduler " << name_ << " is tickling"; }

void Scheduler::Idle() {
  LOG_DEBUG(sys_logger) << "Thread" << GetCurrSysThreadId() << " is idling";
  while (!IsStopable()) {
    // 退出Idle协程
    Coroutine::GetThreadRunningCoroutine()->Yield();
  }
}

void Scheduler::Stop() {
  LOG_DEBUG(sys_logger) << "Scheduler " << name_ << " is stoping";
  if (IsStopable()) {
    return;
  }
  is_stoped_ = true;

  // 如果调度器的创建者线程也参与了调度，那么创建者线程调用Stop时，线程内的调度器必然等于this
  if (use_creator_thread_) {
    ASSERT(GetThreadScheduler().get() == this);
  } else {
    // 如果创建者线程不参与调度，那么创建者线程内的调度器应该为null，必然不等于this
    ASSERT(GetThreadScheduler().get() != this);
    ASSERT(GetThreadScheduler() == nullptr);
  }

  // 全都唤醒一次，让所有线程都从Idle中退出
  for (size_t i = 0; i < thread_count_; i++) {
    Tickle();
  }

  if (use_creator_thread_) {
    creator_schedule_coroutine_->Resume();
    LOG_DEBUG(sys_logger) << " creator schedule coroutine end";
  }

  std::vector<Thread::s_ptr> threads{};
  {
    std::lock_guard<MutexType> lock(mutex_);
    threads.swap(thread_pool_);
  }
  for (auto &thread : threads) {
    thread->Join();
  }
}

void Scheduler::Run() {
  LOG_DEBUG(sys_logger) << "Thread" << GetCurrSysThreadId() << " is running";
  InitThreadScheduler();

  // 如果当前线程不是调度器创建者线程，也就是说当前线程是线程池中的线程，那么需要先为其启动协程模式
  if (GetCurrSysThreadId() != creator_thread_id_) {
    Coroutine::InitThreadToCoMod();
  }
  // 对于线程池中的线程，其RuningCoroutine必然等于主协程
  // 对于调度器的创建者线程，其RunninCoroutine不等于主协程，而是creator_schedule_coroutine_
  thread_schedule_coroutine = Coroutine::GetThreadRunningCoroutine();

  // 创建一个Idle协程，用于线程从Run进入Idle
  auto idle_coroutine =
      std::make_shared<Coroutine>([this] { Idle(); }, 0, true, Coroutine::GetThreadRunningCoroutine());
  // 任务协程，用于取到func任务之后，将func封装进该协程，然后Resume该协程
  auto func_task_coroutine = std::make_shared<Coroutine>(nullptr, 0, true, Coroutine::GetThreadRunningCoroutine());
  // 存储取到的任务
  ScheduleTask task{};

  // 主循环，不停地取任务执行或者进入Idle协程
  while (true) {
    // 清空task
    task.Clear();
    bool tickle_other_thread = false;
    {
      std::lock_guard<MutexType> lock(mutex_);
      for (auto t = task_queue_.begin(); t != task_queue_.end(); t++) {
        // 如果任务的目标线程不是当前线程，那么通知一下其他线程（碰运气随机tickle）
        if (t->target_thread_id_ != -1 && t->target_thread_id_ != GetCurrSysThreadId()) {
          tickle_other_thread = true;
          continue;
        }
        // 取到了任务
        ASSERT(!t->Empty());
        // BUG: hook
        // IO相关的系统调用时，在检测到IO未就绪的情况下，会先添加对应的读写事件，再yield当前协程，等IO就绪后再resume当前协程
        // 多线程高并发情境下，有可能发生刚添加事件就被触发的情况，如果此时当前协程还未来得及yield，则这里就有可能出现协程状态仍为RUNNING的情况
        // 这里简单地跳过这种情况，以损失一点性能为代价，否则整个协程框架都要大改
        if (t->coroutine_ != nullptr && t->coroutine_->GetState() == Coroutine::State::Running) {
          continue;
        }

        // 顺利取到可执行的任务
        task = *t;
        task_queue_.erase(t);
        break;
      }
      // 如果取完任务之后，任务队列非空，那么通知其他线程（碰运气随机tickle）
      tickle_other_thread |= !task_queue_.empty();
    }

    if (tickle_other_thread) {
      Tickle();
    }

    if (task.coroutine_ != nullptr) {
      // 如果任务本身就是一个协程任务，那么直接Resume，当Resume返回时，协程已经执行完毕或者被Yield
      ++active_thread_count_;
      task.coroutine_->Resume();
      --active_thread_count_;
    } else if (task.func_ != nullptr) {
      func_task_coroutine->ResetTaskFunc(task.func_);
      // 执行封装之后的func_task_coroutine
      ++active_thread_count_;
      func_task_coroutine->Resume();
      --active_thread_count_;
    } else {
      // 能够进入这个分支，代表没有取到Task，或者Task中coroutine和func都为空（异常现象）, 那么进入Idle协程
      if (idle_coroutine->GetState() == Coroutine::State::Stop) {
        // 正常情况下，Idle协程会在被线程通知之后，Yield然后回到Run，而不是执行完毕
        // 如果Idle协程已经执行完毕（Idel函数返回），说明调度器已经停止，那么当前Run也应该返回
        LOG_DEBUG(sys_logger) << "Idle coroutine end";
        break;
      }
      // 进入Idle协程，Resume返回时，表明线程在Idle协程内收到了通知，需要回来继续取任务执行
      ++idle_thread_count_;
      idle_coroutine->Resume();
      --idle_thread_count_;
    }
  }
  LOG_DEBUG(sys_logger) << "Thread" << GetCurrSysThreadId() << "Run() is end";
}

template void Scheduler::Schedule(Coroutine::s_ptr sa, int target_thread_id);
template void Scheduler::Schedule(std::function<void()> sa, int target_thread_id);

template auto Scheduler::NonLockScheduleImpl(Coroutine::s_ptr sa, int target_thread_id) -> bool;
template auto Scheduler::NonLockScheduleImpl(std::function<void()> sa, int target_thread_id) -> bool;

}  // namespace wtsclwq
