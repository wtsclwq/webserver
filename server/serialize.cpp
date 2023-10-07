#include "serialize.h"
#include <bits/types/struct_iovec.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <vector>
#include "server/endian.h"
#include "server/log.h"

namespace wtsclwq {
static auto sys_logger = NAMED_LOGGER("system");

ByteArray::Node::Node(size_t size) : data_(new char[size]), size_(size) {}

ByteArray::Node::Node() = default;

ByteArray::Node::~Node() { delete[] data_; }

ByteArray::ByteArray(size_t base_size)
    : node_size_(base_size), capacity_(base_size), root_(new Node(base_size)), cur_node_(root_) {}

ByteArray::~ByteArray() {
  auto cur = root_;
  while (cur != nullptr) {
    auto next = cur->next_;
    delete cur;
    cur = next;
  }
}

auto ByteArray::IsLittleEndian() const -> bool { return endian_ == WTSCLWQ_LITTLE_ENDIAN; }

void ByteArray::SetIsLittleEndian(bool val) {
  if (val) {
    endian_ = WTSCLWQ_LITTLE_ENDIAN;
  } else {
    endian_ = WTSCLWQ_BIG_ENDIAN;
  }
}

void ByteArray::AddCapacity(size_t size) {
  if (size == 0) {
    return;
  }
  // 如果当前容量大于等于需要的容量，直接返回
  size_t remain_cap = GetRemainCapacity();
  if (size <= remain_cap) {
    return;
  }

  // 计算需要扩容的大小
  size -= remain_cap;
  // 计算需要扩容的节点数量
  size_t add_node_count = std::ceil(1.0 * size / node_size_);
  // 新增节点
  // 先找到最后一个节点（因为setpos会导致cur_node_改变位置，所以不能直接使用cur_node）
  auto last = root_;
  while (last->next_ != nullptr) {
    last = last->next_;
  }

  Node *new_cur = nullptr;
  for (size_t i = 0; i < add_node_count; ++i) {
    auto node = new Node(node_size_);
    if (new_cur == nullptr) {
      new_cur = node;
    }
    last->next_ = node;
    last = node;
    capacity_ += node_size_;
  }

  if (remain_cap == 0) {
    cur_node_ = new_cur;
  }
}

void ByteArray::Write(const void *buf, size_t len) {
  if (len == 0) {
    return;
  }

  // 提前尝试扩容，避免多次扩容
  AddCapacity(len);

  // 当前操作的节点内位置
  size_t cur_node_pos = total_cur_pos_ % node_size_;
  // 当前操作的节点剩余容量(byte)
  size_t cur_remain_cap = cur_node_->size_ - cur_node_pos;
  // 当前操作的buf位置
  size_t buf_pos = 0;

  while (len > 0) {
    // 当前节点剩余容量大于等于写入长度，直接写入
    if (cur_remain_cap >= len) {
      memcpy(cur_node_->data_ + cur_node_pos, static_cast<const char *>(buf) + buf_pos, len);
      // 如果当前节点剩余容量等于写入长度，切换到下一个节点
      if (cur_node_->size_ == cur_node_pos + len) {
        cur_node_ = cur_node_->next_;
      }
      total_cur_pos_ += len;
      buf_pos += len;
      len = 0;
    } else {
      // 当前节点剩余容量小于写入长度，写入当前节点剩余容量
      memcpy(cur_node_->data_ + cur_node_pos, static_cast<const char *>(buf) + buf_pos, cur_remain_cap);
      total_cur_pos_ += cur_remain_cap;
      buf_pos += cur_remain_cap;
      len -= cur_remain_cap;
      // 切换到下一个节点
      cur_node_ = cur_node_->next_;
      cur_remain_cap = cur_node_->size_;
      cur_node_pos = 0;
    }
  }

  if (total_cur_pos_ > size_) {
    size_ = total_cur_pos_;
  }
}

void ByteArray::WriteFint8(int8_t value) { Write(&value, sizeof(value)); }

void ByteArray::WriteFuint8(uint8_t value) { Write(&value, sizeof(value)); }

void ByteArray::WriteFint16(int16_t value) {
  // 如果byteorder和当前系统不一致，需要转换
  if (endian_ != WTSCLWQ_BYTE_ORDER) {
    value = Byteswap(value);
  }
  Write(&value, sizeof(value));
}

void ByteArray::WriteFuint16(uint16_t value) {
  if (endian_ != WTSCLWQ_BYTE_ORDER) {
    value = Byteswap(value);
  }
  Write(&value, sizeof(value));
}

void ByteArray::WriteFint32(int32_t value) {
  if (endian_ != WTSCLWQ_BYTE_ORDER) {
    value = Byteswap(value);
  }
  Write(&value, sizeof(value));
}

void ByteArray::WriteFuint32(uint32_t value) {
  if (endian_ != WTSCLWQ_BYTE_ORDER) {
    value = Byteswap(value);
  }
  Write(&value, sizeof(value));
}

void ByteArray::WriteFint64(int64_t value) {
  if (endian_ != WTSCLWQ_BYTE_ORDER) {
    value = Byteswap(value);
  }
  Write(&value, sizeof(value));
}

void ByteArray::WriteFuint64(uint64_t value) {
  if (endian_ != WTSCLWQ_BYTE_ORDER) {
    value = Byteswap(value);
  }
  Write(&value, sizeof(value));
}

static auto EncodeZigZag32(int32_t val) -> uint32_t {
  if (val < 0) {
    return (static_cast<uint32_t>(-val)) * 2 - 1;
  }
  return val * 2;
}

static auto DecodeZigZag32(uint32_t val) -> int32_t { return val >> 1 ^ -(val & 1); }

static auto EncodeZigZag64(int64_t val) -> uint64_t {
  if (val < 0) {
    return (static_cast<uint64_t>(-val)) * 2 - 1;
  }
  return val * 2;
}

static auto DecodeZigZag64(uint64_t val) -> int64_t { return val >> 1 ^ -(val & 1); }

void ByteArray::WriteInt32(int32_t value) { WriteUint32(EncodeZigZag32(value)); }

void ByteArray::WriteUint32(uint32_t value) {
  uint8_t buf[5];
  size_t i = 0;
  while (value >= 0x80) {
    buf[i++] = (value & 0x7f) | 0x80;
    value >>= 7;
  }
  buf[i++] = value;
  Write(buf, i);
}

void ByteArray::WriteInt64(int64_t value) { WriteUint64(EncodeZigZag64(value)); }

void ByteArray::WriteUint64(uint64_t value) {
  uint8_t buf[10];
  size_t i = 0;
  while (value >= 0x80) {
    buf[i++] = (value & 0x7f) | 0x80;
    value >>= 7;
  }
  buf[i++] = value;
  Write(buf, i);
}

void ByteArray::WriteFloat(float value) {
  uint32_t v;
  memcpy(&v, &value, sizeof(value));
  WriteFuint32(v);
}

void ByteArray::WriteDouble(double value) {
  uint64_t v;
  memcpy(&v, &value, sizeof(value));
  WriteFuint64(v);
}

void ByteArray::WriteStringF16(const std::string &value) {
  WriteFuint16(value.size());
  Write(value.c_str(), value.size());
}

void ByteArray::WriteStringF32(const std::string &value) {
  WriteFuint32(value.size());
  Write(value.c_str(), value.size());
}

void ByteArray::WriteStringF64(const std::string &value) {
  WriteFuint64(value.size());
  Write(value.c_str(), value.size());
}

void ByteArray::WriteStringVint(const std::string &value) {
  WriteUint64(value.size());
  Write(value.c_str(), value.size());
}

void ByteArray::WriteStringWithoutLength(const std::string &value) { Write(value.c_str(), value.size()); }

void ByteArray::Read(void *buf, size_t len) {
  if (len > GetReadSize()) {
    throw std::out_of_range("not enough len");
  }
  size_t cur_node_pos = total_cur_pos_ % node_size_;
  size_t cur_remain_cap = cur_node_->size_ - cur_node_pos;
  size_t buf_pos = 0;
  while (len > 0) {
    if (cur_remain_cap >= len) {
      // 如果当前节点剩余容量大于等于读取长度，直接读取
      memcpy(static_cast<char *>(buf) + buf_pos, cur_node_->data_ + cur_node_pos, len);
      // 如果当前节点剩余容量等于读取长度，切换到下一个节点
      if (cur_remain_cap == len) {
        cur_node_ = cur_node_->next_;
      }
      total_cur_pos_ += len;
      buf_pos += len;
      len = 0;
    } else {
      // 如果当前节点剩余容量小于读取长度，读取当前节点剩余容量
      memcpy(static_cast<char *>(buf) + buf_pos, cur_node_->data_ + cur_node_pos, cur_remain_cap);
      total_cur_pos_ += cur_remain_cap;
      buf_pos += cur_remain_cap;
      len -= cur_remain_cap;
      // 切换到下一个节点
      cur_node_ = cur_node_->next_;
      cur_remain_cap = cur_node_->size_;
      cur_node_pos = 0;
    }
  }
}

void ByteArray::PosRead(void *buf, size_t len, size_t position) const {
  if (len > (size_ - position)) {
    throw std::out_of_range("not enough len");
  }

  size_t cur_node_pos = position % node_size_;
  size_t cur_remain_cap = cur_node_->size_ - cur_node_pos;
  size_t buf_pos = 0;

  Node *temp = cur_node_;
  while (len > 0) {
    if (cur_remain_cap >= len) {
      // 如果当前节点剩余容量大于等于读取长度，直接读取
      memcpy(static_cast<char *>(buf) + buf_pos, temp->data_ + cur_node_pos, len);
      if (cur_remain_cap == len) {
        temp = temp->next_;
      }
      buf_pos += len;
      len = 0;
    } else {
      // 如果当前节点剩余容量小于读取长度，读取当前节点剩余容量
      memcpy(static_cast<char *>(buf) + buf_pos, temp->data_ + cur_node_pos, cur_remain_cap);
      buf_pos += cur_remain_cap;
      len -= cur_remain_cap;
      // 切换到下一个节点
      temp = temp->next_;
      cur_remain_cap = temp->size_;
      cur_node_pos = 0;
    }
  }
}

auto ByteArray::ReadFint8() -> int8_t {
  int8_t v;
  Read(&v, sizeof(v));
  return v;
}

auto ByteArray::ReadFuint8() -> uint8_t {
  uint8_t v;
  Read(&v, sizeof(v));
  return v;
}

auto ByteArray::ReadFint16() -> int16_t {
  int16_t v;
  Read(&v, sizeof(v));
  if (endian_ != WTSCLWQ_BYTE_ORDER) {
    v = Byteswap(v);
  }
  return v;
}

auto ByteArray::ReadFuint16() -> uint16_t {
  uint16_t v;
  Read(&v, sizeof(v));
  if (endian_ != WTSCLWQ_BYTE_ORDER) {
    v = Byteswap(v);
  }
  return v;
}

auto ByteArray::ReadFint32() -> int32_t {
  int32_t v;
  Read(&v, sizeof(v));
  if (endian_ != WTSCLWQ_BYTE_ORDER) {
    v = Byteswap(v);
  }
  return v;
}

auto ByteArray::ReadFuint32() -> uint32_t {
  uint32_t v;
  Read(&v, sizeof(v));
  if (endian_ != WTSCLWQ_BYTE_ORDER) {
    v = Byteswap(v);
  }
  return v;
}

auto ByteArray::ReadFint64() -> int64_t {
  int64_t v;
  Read(&v, sizeof(v));
  if (endian_ != WTSCLWQ_BYTE_ORDER) {
    v = Byteswap(v);
  }
  return v;
}

auto ByteArray::ReadFuint64() -> uint64_t {
  uint64_t v;
  Read(&v, sizeof(v));
  if (endian_ != WTSCLWQ_BYTE_ORDER) {
    v = Byteswap(v);
  }
  return v;
}

auto ByteArray::ReadInt32() -> int32_t { return DecodeZigZag32(ReadUint32()); }

auto ByteArray::ReadUint32() -> uint32_t {
  uint32_t result = 0;
  for (int i = 0; i < 32; i += 7) {
    uint8_t b = ReadFuint8();
    if (b < 0x80) {
      result |= static_cast<uint32_t>(b) << i;
      break;
    }
    result |= static_cast<uint32_t>(b & 0x7f) << i;
  }
  return result;
}

auto ByteArray::ReadInt64() -> int64_t { return DecodeZigZag64(ReadUint64()); }

auto ByteArray::ReadUint64() -> uint64_t {
  uint64_t result = 0;
  for (int i = 0; i < 64; i += 7) {
    uint8_t b = ReadFuint8();
    if (b < 0x80) {
      result |= static_cast<uint64_t>(b) << i;
      break;
    }
    result |= static_cast<uint64_t>(b & 0x7f) << i;
  }
  return result;
}

auto ByteArray::ReadFloat() -> float {
  uint32_t v = ReadFuint32();
  float value;
  memcpy(&value, &v, sizeof(v));
  return value;
}

auto ByteArray::ReadDouble() -> double {
  uint64_t v = ReadFuint64();
  double value;
  memcpy(&value, &v, sizeof(v));
  return value;
}

auto ByteArray::ReadStringF16() -> std::string {
  uint16_t len = ReadFuint16();
  std::string result;
  result.resize(len);
  Read(result.data(), len);
  return result;
}

auto ByteArray::ReadStringF32() -> std::string {
  uint32_t len = ReadFuint32();
  std::string result;
  result.resize(len);
  Read(result.data(), len);
  return result;
}

auto ByteArray::ReadStringF64() -> std::string {
  uint64_t len = ReadFuint64();
  std::string result;
  result.resize(len);
  Read(result.data(), len);
  return result;
}

auto ByteArray::ReadStringVint() -> std::string {
  uint64_t len = ReadUint64();
  std::string result;
  result.resize(len);
  Read(result.data(), len);
  return result;
}

void ByteArray::Clear() {
  size_ = 0;
  total_cur_pos_ = 0;
  Node *cur = root_->next_;
  while (cur != nullptr) {
    auto next = cur->next_;
    delete cur;
    cur = next;
  }
  cur_node_ = root_;
  root_->next_ = nullptr;
}

void ByteArray::SetPosition(size_t v) {
  if (v > size_) {
    throw std::out_of_range("set position out of range");
  }

  total_cur_pos_ = v;
  if (total_cur_pos_ > size_) {
    size_ = total_cur_pos_;
  }

  cur_node_ = root_;
  while (v > cur_node_->size_) {
    v -= cur_node_->size_;
    cur_node_ = cur_node_->next_;
  }

  if (v == cur_node_->size_) {
    cur_node_ = cur_node_->next_;
  }
}

auto ByteArray::WriteToFile(const std::string &name) const -> bool {
  std::ofstream ofs;
  ofs.open(name, std::ios::trunc | std::ios::binary);
  if (!ofs.is_open()) {
    LOG_INFO(sys_logger) << "write to file name=" << name << " failed";
    return false;
  }

  size_t read_size = GetReadSize();
  size_t pos = total_cur_pos_;
  Node *cur = cur_node_;
  while (read_size > 0) {
    size_t len = cur->size_ - pos;
    if (len > read_size) {
      len = read_size;
    }
    ofs.write(cur->data_ + pos, len);
    if (cur->size_ == pos + len) {
      cur = cur->next_;
      pos = 0;
    } else {
      pos += len;
    }
    read_size -= len;
  }
  return true;
}

auto ByteArray::ReadFromFile(const std::string &name) -> bool {
  std::ifstream ifs;
  ifs.open(name, std::ios::binary);
  if (!ifs.is_open()) {
    LOG_INFO(sys_logger) << "read from file name=" << name << " failed";
    return false;
  }

  auto buff = std::make_unique<char[]>(node_size_);
  while (!ifs.eof()) {
    ifs.read(buff.get(), node_size_);
    Write(buff.get(), ifs.gcount());
  }
  return true;
}

auto ByteArray::ToString() const -> std::string {
  std::string str;
  str.resize(GetReadSize());
  if (str.empty()) {
    return str;
  }
  PosRead(str.data(), str.size(), total_cur_pos_);
  return str;
}

auto ByteArray::ToHexString() const -> std::string {
  std::string str = ToString();
  std::stringstream ss;
  for (size_t i = 0; i < str.size(); ++i) {
    if (i > 0 && i % 32 == 0) {
      ss << std::endl;
    }
    ss << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(static_cast<uint8_t>(str[i])) << " ";
  }
  return ss.str();
}

auto ByteArray::GetReadableBuffers(std::vector<iovec> *buffers, size_t len) const -> size_t {
  len = std::min(len, GetReadSize());
  if (len == 0) {
    return 0;
  }

  size_t size = len;
  size_t cur_node_pos = total_cur_pos_ % node_size_;
  size_t cur_remain_cap = cur_node_->size_ - cur_node_pos;

  struct iovec iov;
  Node *temp = cur_node_;
  while (len > 0) {
    // 当前节点剩余容量大于等于读取长度，直接读取
    if (cur_remain_cap >= len) {
      iov.iov_base = temp->data_ + cur_node_pos;
      iov.iov_len = len;
      len = 0;
    } else {
      // 如果当前节点剩余容量小于读取长度，读取当前节点剩余容量
      iov.iov_base = temp->data_ + cur_node_pos;
      iov.iov_len = cur_remain_cap;
      len -= cur_remain_cap;
      temp = temp->next_;
      cur_remain_cap = temp->size_;
      cur_node_pos = 0;
    }
    buffers->push_back(iov);
  }
  return size;
}

auto ByteArray::GetPosReadableBuffers(std::vector<iovec> *buffers, uint64_t len, uint64_t position) const -> uint64_t {
  len = std::min(len, size_ - position);
  if (len == 0) {
    return 0;
  }

  size_t size = len;
  size_t cur_node_pos = position % node_size_;
  size_t count = position / node_size_;
  Node *temp = root_;
  while (count > 0) {
    temp = temp->next_;
    --count;
  }

  size_t cur_remain_cap = temp->size_ - cur_node_pos;
  struct iovec iov;
  while (len > 0) {
    // 当前节点剩余容量大于等于读取长度，直接读取
    if (cur_remain_cap >= len) {
      iov.iov_base = temp->data_ + cur_node_pos;
      iov.iov_len = len;
      len = 0;
    } else {
      // 如果当前节点剩余容量小于读取长度，读取当前节点剩余容量
      iov.iov_base = temp->data_ + cur_node_pos;
      iov.iov_len = cur_remain_cap;
      len -= cur_remain_cap;
      temp = temp->next_;
      cur_remain_cap = temp->size_;
      cur_node_pos = 0;
    }
    buffers->push_back(iov);
  }
  return size;
}

auto ByteArray::GetWriteableBuffers(std::vector<iovec> *buffers, uint64_t len) -> uint64_t {
  if (len == 0) {
    return 0;
  }
  AddCapacity(len);

  size_t size = len;
  size_t cur_node_pos = total_cur_pos_ % node_size_;
  size_t cur_remain_cap = cur_node_->size_ - cur_node_pos;
  struct iovec iov;
  Node *temp = cur_node_;
  while (len > 0) {
    // 当前节点剩余容量大于等于读取长度，直接读取
    if (cur_remain_cap >= len) {
      iov.iov_base = temp->data_ + cur_node_pos;
      iov.iov_len = len;
      len = 0;
    } else {
      // 如果当前节点剩余容量小于读取长度，读取当前节点剩余容量
      iov.iov_base = temp->data_ + cur_node_pos;
      iov.iov_len = cur_remain_cap;
      len -= cur_remain_cap;
      temp = temp->next_;
      cur_remain_cap = temp->size_;
      cur_node_pos = 0;
    }
    buffers->push_back(iov);
  }
  return size;
}

}  // namespace wtsclwq