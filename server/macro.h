#include "log.h"
#include "utils.h"
#include <cassert>

#define ASSERT(x)                                                                                                    \
  if (!(x)) {                                                                                                        \
    LOG_ERROR(ROOT_LOGGER) << "assert failed: " << #x << "backtrace\n" << wtsclwq::BacktraceToString(100, 2, "   "); \
    assert(x);                                                                                                       \
  }