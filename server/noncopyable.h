#ifndef _WTSCLWQ_NONCOPYABLE_
#define _WTSCLWQ_NONCOPYABLE_

namespace wtsclwq {
class Noncopyable {
 public:
  Noncopyable() = default;

  virtual ~Noncopyable() = default;

  Noncopyable(const Noncopyable &) = delete;

  auto operator=(const Noncopyable &) -> Noncopyable & = delete;
};
}  // namespace wtsclwq

#endif