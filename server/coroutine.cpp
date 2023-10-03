#include "coroutine.h"
#include <ucontext.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include "config.h"
#include "log.h"
#include "macro.h"

namespace wtsclwq {
static auto sys_logger = NAMED_LOGGER("system");

// 用于生成协程id
static std::atomic<uint64_t> next_coroutine_id{0};

// 用于统计当前系统中的协程数量
static std::atomic<uint64_t> system_coroutine_count{0};

// 用于存储当前线程中正在执行的协程
static thread_local Coroutine::s_ptr thread_running_coroutine = nullptr;

// 用于存储当前线程中的主协程(非对称协程，相当于主干协程，任务协程都是从主协程中分裂出来的)
static thread_local Coroutine::s_ptr thread_main_coroutine = nullptr;

// 协程栈大小，可以通过配置文件进行配置
static auto coroutine_stack_size =
    ConfigMgr::GetInstance() -> GetOrAddDefaultConfigItem("coroutine.stack_size", 128 * 1024, "fiber stack size");

class MallocStackAlloctor {
 public:
  static auto Alloc(size_t size) -> void * { return std::malloc(size); }

  static void Dealloc(void *p) { free(p); }
};

using StackAlloctor = MallocStackAlloctor;

Coroutine::Coroutine() {
  // 初始化调用构造函数的线程的主协程
  state_ = Running;
  id_ = next_coroutine_id++;
  ++system_coroutine_count;
  // 初始化context_t对象，用来swapcontext(见man swapcontext)
  if (getcontext(&context_) != 0) {
    LOG_ERROR(sys_logger) << "getcontext failed";
    throw std::runtime_error("getcontext failed");
  }
  LOG_DEBUG(sys_logger) << "Coroutine " << id_ << " created";
}

Coroutine::Coroutine(std::function<void()> task, uint32_t stack_size, bool has_parent, const s_ptr &parent)
    : id_(next_coroutine_id++),
      stack_size_(stack_size == 0 ? coroutine_stack_size->GetValue() : stack_size),
      task_func_(std::move(task)),
      parent_(parent),
      has_parent_(has_parent) {
  ++system_coroutine_count;
  stack_ = StackAlloctor::Alloc(stack_size_);

  if (stack_ == nullptr) {
    LOG_ERROR(sys_logger) << "alloc stack failed";
    throw std::runtime_error("alloc stack failed");
  }

  if (getcontext(&context_) == -1) {
    LOG_ERROR(sys_logger) << "getcontext failed";
    throw std::runtime_error("getcontext failed");
  }

  context_.uc_stack.ss_sp = stack_;         // 栈指针 sp
  context_.uc_stack.ss_size = stack_size_;  // 栈大小
  context_.uc_link = nullptr;

  makecontext(&context_, &MainFunc, 0);
  LOG_DEBUG(sys_logger) << "Coroutine " << id_ << " created";
}

Coroutine::~Coroutine() {
  // stack_非空，说明this是一个子协程，需要释放栈空间并且确保协程结束
  if (stack_ != nullptr) {
    ASSERT(state_ == State::Stop);
    StackAlloctor::Dealloc(stack_);
  } else {
    // stack_为空，说明this是一个主协程
    ASSERT(state_ == State::Running);  // 主协程销毁时，必然处于运行状态·
    ASSERT(task_func_ == nullptr);          // 主协程没有task_
    Coroutine *curr = thread_running_coroutine.get();
    if (curr == this) {
      SetThreadRunningCoroutine(
          nullptr);  // 将当前线程中正在执行的协程置空, 表示线程已经没有协程在执行了（退出协程模式）
    }
  }
  --system_coroutine_count;
  LOG_DEBUG(sys_logger) << "Coroutine " << id_ << " destroyed";
}

void Coroutine::ResetTaskFunc(std::function<void()> new_task_func) {
  ASSERT(state_ == State::Stop);  // 简化状态管理，只有Stop状态的协程才能被重置，正常实现的话，会有Init状态，也能被重置
  ASSERT(stack_ != nullptr);      // 只有子协程才能被重置

  task_func_ = std::move(new_task_func);
  if (getcontext(&context_) == -1) {
    LOG_ERROR(sys_logger) << "getcontext failed";
    ASSERT(false);
  }

  // 重置协程上下文
  context_.uc_stack.ss_sp = stack_;
  context_.uc_stack.ss_size = stack_size_;
  context_.uc_link = nullptr;
  makecontext(&context_, &MainFunc, 0);

  state_ = State::Ready;
}

void Coroutine::Resume() {
  ASSERT(state_ == State::Ready);                       // 只有Ready状态的协程才能被Resume
  SetThreadRunningCoroutine(this->shared_from_this());  // 将当前线程中正在执行的协程置为this

  state_ = State::Running;

  ASSERT(has_parent_);
  auto parent_ptr = parent_.lock();
  Coroutine *raw_ptr = parent_ptr.get();
  ASSERT(parent_ptr != nullptr);
  parent_ptr.reset();
  if (swapcontext(&raw_ptr->context_, &context_) == -1) {
    ASSERT(false);
  }
}

void Coroutine::Yield() {
  ASSERT((state_ == Running || state_ == Stop));
  if (state_ != Stop) {
    state_ = State::Ready;
  }
  ASSERT(has_parent_);
  auto parent_ptr = parent_.lock();
  Coroutine *raw_ptr = parent_ptr.get();
  ASSERT(parent_ptr != nullptr);
  SetThreadRunningCoroutine(parent_ptr);
  parent_ptr.reset();
  if (swapcontext(&context_, &raw_ptr->context_) == -1) {
    ASSERT(false);
  }
}

auto Coroutine::GetId() const -> uint64_t { return id_; }

auto Coroutine::GetState() const -> State { return state_; }

void Coroutine::InitThreadToCoMod() {
  if (thread_main_coroutine == nullptr) {
    thread_main_coroutine = std::shared_ptr<Coroutine>(new Coroutine);  // 创建主协程，这里调用了无参构造函数
    SetThreadRunningCoroutine(thread_main_coroutine);
  }
}

void Coroutine::SetThreadRunningCoroutine(Coroutine::s_ptr curr) { thread_running_coroutine = std::move(curr); }

auto Coroutine::GetThreadRunningCoroutine() -> s_ptr { return thread_running_coroutine; }

auto Coroutine::GetThreadMainCoroutine() -> s_ptr { return thread_main_coroutine; }

void Coroutine::MainFunc() {
  Coroutine::s_ptr curr = GetThreadRunningCoroutine();  // 获取当前线程中正在执行的协程
  ASSERT(curr != nullptr);
  ASSERT(curr->task_func_ != nullptr);

  curr->task_func_();  // 执行协程的具体任务
  curr->state_ = State::Stop;
  curr->task_func_ = nullptr;

  Coroutine *raw_ptr = curr.get();
  curr.reset();
  raw_ptr->Yield();
}
}  // namespace wtsclwq