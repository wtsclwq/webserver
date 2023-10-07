#include "stream.h"

namespace wtsclwq {

Stream::~Stream() = default;

auto Stream::ReadFixSize(void *buffer, size_t length) -> int {
  size_t offset = 0;
  while (offset < length) {
    auto len = Read(static_cast<char *>(buffer) + offset, length - offset);
    if (len <= 0) {
      return len;
    }
    offset += len;
  }
  return offset;
}

auto Stream::ReadFixSizeToByteArray(const ByteArray::s_ptr &ba, size_t length) -> int {
  size_t left = length;
  while (left < length) {
    auto len = ReadToByteArray(ba, left);
    if (len <= 0) {
      return len;
    }
    left -= len;
  }
  return length;
}

}  // namespace wtsclwq