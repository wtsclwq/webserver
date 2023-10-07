#include "stream.h"

namespace wtsclwq {

Stream::~Stream() = default;

auto Stream::ReadFixSize(void *buffer, size_t length) -> int {
  size_t offset = 0;
  int64_t left = length;
  while (left > 0) {
    int64_t len = Read(static_cast<char *>(buffer) + offset, left);
    if (len <= 0) {
      return len;
    }
    offset += len;
    left -= len;
  }
  return length;
}

auto Stream::ReadFixSizeToByteArray(const ByteArray::s_ptr &ba, size_t length) -> int {
  int64_t left = length;
  while (left > 0) {
    int64_t len = ReadToByteArray(ba, left);
    if (len <= 0) {
      return len;
    }
    left -= len;
  }
  return length;
}

auto Stream::WriteFixSize(const void *buffer, size_t length) -> int {
  size_t offset = 0;
  int64_t left = length;
  while (left > 0) {
    int64_t len = Write(static_cast<const char *>(buffer) + offset, left);
    if (len <= 0) {
      return len;
    }
    offset += len;
    left -= len;
  }
  return length;
}

auto Stream::WriteFixSizeFromByteArray(const ByteArray::s_ptr &ba, size_t length) -> int {
  int64_t left = length;
  while (left > 0) {
    int64_t len = WriteFromByteArray(ba, left);
    if (len <= 0) {
      return len;
    }
    left -= len;
  }
  return length;
}

}  // namespace wtsclwq