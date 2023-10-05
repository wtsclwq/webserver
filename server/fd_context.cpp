#include "fd_context.h"
#include "macro.h"

namespace wtsclwq {
auto FileDescContext::GetEventContext(EventType event_type) -> FileDescContext::EventContext::s_ptr {
  switch (event_type) {
    case EventType::Read:
      return read_event_ctx_;
    case EventType::Write:
      return write_event_ctx_;
    default:
      ASSERT(false);
  }
  return nullptr;
}

void FileDescContext::ResetEventContext(EventType event_type) {
  EventContext::s_ptr target_ctx = GetEventContext(event_type);
  target_ctx->Reset();
}

void FileDescContext::TriggerEvent(EventType event_type) {
  // 要触发的事件必须是注册过的
  ASSERT((registered_event_types_ & event_type) != 0);

  EventContext::s_ptr target_ctx = GetEventContext(event_type);
  Scheduler::s_ptr scheduler = target_ctx->scheduler_.lock();
  if (scheduler == nullptr) {
    return;
  }
  if (target_ctx->func_ != nullptr) {
    scheduler->Schedule(target_ctx->func_);
  } else {
    scheduler->Schedule(target_ctx->coroutine_);
  }
  // 触发完事件后，重置事件上下文，复用
  target_ctx->Reset();
}

void FileDescContext::EventContext::Reset() {
  scheduler_.reset();
  coroutine_.reset();
  func_ = nullptr;
}

}  // namespace wtsclwq