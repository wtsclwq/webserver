#ifndef _WTSCLWQ_SERIALIZE_
#define _WTSCLWQ_SERIALIZE_

#include <sys/socket.h>
#include <sys/types.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "endian.h"

namespace wtsclwq {

/**
 * @brief 二进制数组,提供基础类型的序列化,反序列化功能
 */
class ByteArray {
 public:
  using s_ptr = std::shared_ptr<ByteArray>;

  /**
   * @brief ByteArray的存储节点
   */
  struct Node {
    /**
     * @brief 构造指定大小的内存块
     * @param[in] s 内存块字节数
     */
    explicit Node(size_t s);

    /**
     * 无参构造函数
     */
    Node();

    /**
     * 析构函数,释放内存
     */
    ~Node();

    /// 内存块地址指针
    char *data_{};
    /// 下一个内存块地址
    Node *next_{};
    /// 内存块大小
    size_t size_{};
  };

  /**
   * @brief 使用指定长度的内存块构造ByteArray
   * @param[in] base_size 内存块大小
   */
  explicit ByteArray(size_t base_size = 4096);

  /**
   * @brief 析构函数
   */
  ~ByteArray();

  /**
   * @brief 写入固定长度int8_t类型的数据
   * @post m_position += sizeof(value)
   *       如果m_position > m_size 则 m_size = m_position
   */
  void WriteFint8(int8_t value);
  /**
   * @brief 写入固定长度uint8_t类型的数据
   * @post m_position += sizeof(value)
   *       如果m_position > m_size 则 m_size = m_position
   */
  void WriteFuint8(uint8_t value);
  /**
   * @brief 写入固定长度int16_t类型的数据(大端/小端)
   * @post m_position += sizeof(value)
   *       如果m_position > m_size 则 m_size = m_position
   */
  void WriteFint16(int16_t value);
  /**
   * @brief 写入固定长度uint16_t类型的数据(大端/小端)
   * @post m_position += sizeof(value)
   *       如果m_position > m_size 则 m_size = m_position
   */
  void WriteFuint16(uint16_t value);

  /**
   * @brief 写入固定长度int32_t类型的数据(大端/小端)
   * @post m_position += sizeof(value)
   *       如果m_position > m_size 则 m_size = m_position
   */
  void WriteFint32(int32_t value);

  /**
   * @brief 写入固定长度uint32_t类型的数据(大端/小端)
   * @post m_position += sizeof(value)
   *       如果m_position > m_size 则 m_size = m_position
   */
  void WriteFuint32(uint32_t value);

  /**
   * @brief 写入固定长度int64_t类型的数据(大端/小端)
   * @post m_position += sizeof(value)
   *       如果m_position > m_size 则 m_size = m_position
   */
  void WriteFint64(int64_t value);

  /**
   * @brief 写入固定长度uint64_t类型的数据(大端/小端)
   * @post m_position += sizeof(value)
   *       如果m_position > m_size 则 m_size = m_position
   */
  void WriteFuint64(uint64_t value);

  /**
   * @brief 写入有符号Varint32类型的数据
   * @post m_position += 实际占用内存(1 ~ 5)
   *       如果m_position > m_size 则 m_size = m_position
   */
  void WriteInt32(int32_t value);
  /**
   * @brief 写入无符号Varint32类型的数据
   * @post m_position += 实际占用内存(1 ~ 5)
   *       如果m_position > m_size 则 m_size = m_position
   */
  void WriteUint32(uint32_t value);

  /**
   * @brief 写入有符号Varint64类型的数据
   * @post m_position += 实际占用内存(1 ~ 10)
   *       如果m_position > m_size 则 m_size = m_position
   */
  void WriteInt64(int64_t value);

  /**
   * @brief 写入无符号Varint64类型的数据
   * @post m_position += 实际占用内存(1 ~ 10)
   *       如果m_position > m_size 则 m_size = m_position
   */
  void WriteUint64(uint64_t value);

  /**
   * @brief 写入float类型的数据
   * @post m_position += sizeof(value)
   *       如果m_position > m_size 则 m_size = m_position
   */
  void WriteFloat(float value);

  /**
   * @brief 写入double类型的数据
   * @post m_position += sizeof(value)
   *       如果m_position > m_size 则 m_size = m_position
   */
  void WriteDouble(double value);

  /**
   * @brief 写入std::string类型的数据,用uint16_t作为长度类型
   * @post m_position += 2 + value.size()
   *       如果m_position > m_size 则 m_size = m_position
   */
  void WriteStringF16(const std::string &value);

  /**
   * @brief 写入std::string类型的数据,用uint32_t作为长度类型
   * @post m_position += 4 + value.size()
   *       如果m_position > m_size 则 m_size = m_position
   */
  void WriteStringF32(const std::string &value);

  /**
   * @brief 写入std::string类型的数据,用uint64_t作为长度类型
   * @post m_position += 8 + value.size()
   *       如果m_position > m_size 则 m_size = m_position
   */
  void WriteStringF64(const std::string &value);

  /**
   * @brief 写入std::string类型的数据,用无符号Varint64作为长度类型
   * @post m_position += Varint64长度 + value.size()
   *       如果m_position > m_size 则 m_size = m_position
   */
  void WriteStringVint(const std::string &value);

  /**
   * @brief 写入std::string类型的数据,无长度
   * @post m_position += value.size()
   *       如果m_position > m_size 则 m_size = m_position
   */
  void WriteStringWithoutLength(const std::string &value);

  /**
   * @brief 读取int8_t类型的数据
   * @pre getReadSize() >= sizeof(int8_t)
   * @post m_position += sizeof(int8_t);
   * @exception 如果getReadSize() < sizeof(int8_t) 抛出 std::out_of_range
   */
  auto ReadFint8() -> int8_t;

  /**
   * @brief 读取uint8_t类型的数据
   * @pre getReadSize() >= sizeof(uint8_t)
   * @post m_position += sizeof(uint8_t);
   * @exception 如果getReadSize() < sizeof(uint8_t) 抛出 std::out_of_range
   */
  auto ReadFuint8() -> uint8_t;

  /**
   * @brief 读取int16_t类型的数据
   * @pre getReadSize() >= sizeof(int16_t)
   * @post m_position += sizeof(int16_t);
   * @exception 如果getReadSize() < sizeof(int16_t) 抛出 std::out_of_range
   */
  auto ReadFint16() -> int16_t;

  /**
   * @brief 读取uint16_t类型的数据
   * @pre getReadSize() >= sizeof(uint16_t)
   * @post m_position += sizeof(uint16_t);
   * @exception 如果getReadSize() < sizeof(uint16_t) 抛出 std::out_of_range
   */
  auto ReadFuint16() -> uint16_t;

  /**
   * @brief 读取int32_t类型的数据
   * @pre getReadSize() >= sizeof(int32_t)
   * @post m_position += sizeof(int32_t);
   * @exception 如果getReadSize() < sizeof(int32_t) 抛出 std::out_of_range
   */
  auto ReadFint32() -> int32_t;

  /**
   * @brief 读取uint32_t类型的数据
   * @pre getReadSize() >= sizeof(uint32_t)
   * @post m_position += sizeof(uint32_t);
   * @exception 如果getReadSize() < sizeof(uint32_t) 抛出 std::out_of_range
   */
  auto ReadFuint32() -> uint32_t;

  /**
   * @brief 读取int64_t类型的数据
   * @pre getReadSize() >= sizeof(int64_t)
   * @post m_position += sizeof(int64_t);
   * @exception 如果getReadSize() < sizeof(int64_t) 抛出 std::out_of_range
   */
  auto ReadFint64() -> int64_t;

  /**
   * @brief 读取uint64_t类型的数据
   * @pre getReadSize() >= sizeof(uint64_t)
   * @post m_position += sizeof(uint64_t);
   * @exception 如果getReadSize() < sizeof(uint64_t) 抛出 std::out_of_range
   */
  auto ReadFuint64() -> uint64_t;

  /**
   * @brief 读取有符号Varint32类型的数据
   * @pre getReadSize() >= 有符号Varint32实际占用内存
   * @post m_position += 有符号Varint32实际占用内存
   * @exception 如果getReadSize() < 有符号Varint32实际占用内存 抛出 std::out_of_range
   */
  auto ReadInt32() -> int32_t;

  /**
   * @brief 读取无符号Varint32类型的数据
   * @pre getReadSize() >= 无符号Varint32实际占用内存
   * @post m_position += 无符号Varint32实际占用内存
   * @exception 如果getReadSize() < 无符号Varint32实际占用内存 抛出 std::out_of_range
   */
  auto ReadUint32() -> uint32_t;

  /**
   * @brief 读取有符号Varint64类型的数据
   * @pre getReadSize() >= 有符号Varint64实际占用内存
   * @post m_position += 有符号Varint64实际占用内存
   * @exception 如果getReadSize() < 有符号Varint64实际占用内存 抛出 std::out_of_range
   */
  auto ReadInt64() -> int64_t;

  /**
   * @brief 读取无符号Varint64类型的数据
   * @pre getReadSize() >= 无符号Varint64实际占用内存
   * @post m_position += 无符号Varint64实际占用内存
   * @exception 如果getReadSize() < 无符号Varint64实际占用内存 抛出 std::out_of_range
   */
  auto ReadUint64() -> uint64_t;

  /**
   * @brief 读取float类型的数据
   * @pre getReadSize() >= sizeof(float)
   * @post m_position += sizeof(float);
   * @exception 如果getReadSize() < sizeof(float) 抛出 std::out_of_range
   */
  auto ReadFloat() -> float;

  /**
   * @brief 读取double类型的数据
   * @pre getReadSize() >= sizeof(double)
   * @post m_position += sizeof(double);
   * @exception 如果getReadSize() < sizeof(double) 抛出 std::out_of_range
   */
  auto ReadDouble() -> double;

  /**
   * @brief 读取std::string类型的数据,用uint16_t作为长度
   * @pre getReadSize() >= sizeof(uint16_t) + size
   * @post m_position += sizeof(uint16_t) + size;
   * @exception 如果getReadSize() < sizeof(uint16_t) + size 抛出 std::out_of_range
   */
  auto ReadStringF16() -> std::string;

  /**
   * @brief 读取std::string类型的数据,用uint32_t作为长度
   * @pre getReadSize() >= sizeof(uint32_t) + size
   * @post m_position += sizeof(uint32_t) + size;
   * @exception 如果getReadSize() < sizeof(uint32_t) + size 抛出 std::out_of_range
   */
  auto ReadStringF32() -> std::string;

  /**
   * @brief 读取std::string类型的数据,用uint64_t作为长度
   * @pre getReadSize() >= sizeof(uint64_t) + size
   * @post m_position += sizeof(uint64_t) + size;
   * @exception 如果getReadSize() < sizeof(uint64_t) + size 抛出 std::out_of_range
   */
  auto ReadStringF64() -> std::string;

  /**
   * @brief 读取std::string类型的数据,用无符号Varint64作为长度
   * @pre getReadSize() >= 无符号Varint64实际大小 + size
   * @post m_position += 无符号Varint64实际大小 + size;
   * @exception 如果getReadSize() < 无符号Varint64实际大小 + size 抛出 std::out_of_range
   */
  auto ReadStringVint() -> std::string;

  /**
   * @brief 清空ByteArray
   * @post m_position = 0, m_size = 0
   */
  void Clear();

  /**
   * @brief 写入len长度的数据
   * @param[in] buf 内存缓存指针
   * @param[in] len 数据大小
   * @post m_position += len, 如果m_position > m_size 则 m_size = m_position
   */
  void Write(const void *buf, size_t len);

  /**
   * @brief 读取len长度的数据
   * @param[out] buf 内存缓存指针
   * @param[in] size 数据大小
   * @post m_position += len, 如果m_position > m_size 则 m_size = m_position
   * @exception 如果getReadSize() < len 则抛出 std::out_of_range
   */
  void Read(void *buf, size_t len);

  /**
   * @brief 从Pos开始，读取len长度的数据
   * @param[out] buf 内存缓存指针
   * @param[in] len 数据大小
   * @param[in] position 读取开始位置
   * @exception 如果 (m_size - position) < len 则抛出 std::out_of_range
   */
  void PosRead(void *buf, size_t len, size_t position) const;

  /**
   * @brief 返回ByteArray当前位置
   */
  auto GetPosition() const -> size_t { return total_cur_pos_; }

  /**
   * @brief 设置ByteArray当前位置
   * @post 如果m_position > m_size 则 m_size = m_position
   * @exception 如果m_position > m_capacity 则抛出 std::out_of_range
   */
  void SetPosition(size_t v);

  /**
   * @brief 把ByteArray的数据写入到文件中
   * @param[in] name 文件名
   */
  auto WriteToFile(const std::string &name) const -> bool;

  /**
   * @brief 从文件中读取数据
   * @param[in] name 文件名
   */
  auto ReadFromFile(const std::string &name) -> bool;

  /**
   * @brief 返回内存块的大小
   */
  auto GetBaseSize() const -> size_t { return node_size_; }

  /**
   * @brief 返回可读取数据大小
   */
  auto GetReadSize() const -> size_t { return size_ - total_cur_pos_; }

  /**
   * @brief 是否是小端
   */
  auto IsLittleEndian() const -> bool;

  /**
   * @brief 设置是否为小端
   */
  void SetIsLittleEndian(bool val);

  /**
   * @brief 将ByteArray里面的数据[m_position, m_size)转成std::string
   */
  auto ToString() const -> std::string;

  /**
   * @brief 将ByteArray里面的数据[m_position, m_size)转成16进制的std::string(格式:FF FF FF)
   */
  auto ToHexString() const -> std::string;

  /**
   * @brief 获取可读取的缓存,保存成iovec数组
   * @param[out] buffers 保存可读取数据的iovec数组
   * @param[in] len 读取数据的长度,如果len > getReadSize() 则 len = getReadSize()
   * @return 返回实际数据的长度
   */
  auto GetReadableBuffers(std::vector<iovec> *buffers, uint64_t len = UINT64_MAX) const -> uint64_t;

  /**
   * @brief 获取可读取的缓存,保存成iovec数组,从position位置开始
   * @param[out] buffers 保存可读取数据的iovec数组
   * @param[in] len 读取数据的长度,如果len > getReadSize() 则 len = getReadSize()
   * @param[in] position 读取数据的位置
   * @return 返回实际数据的长度
   */
  auto GetPosReadableBuffers(std::vector<iovec> *buffers, uint64_t len, uint64_t position) const -> uint64_t;

  /**
   * @brief 获取可写入的缓存,保存成iovec数组
   * @param[out] buffers 保存可写入的内存的iovec数组
   * @param[in] len 写入的长度
   * @return 返回实际的长度
   * @post 如果(m_position + len) > m_capacity 则 m_capacity扩容N个节点以容纳len长度
   */
  auto GetWriteableBuffers(std::vector<iovec> *buffers, uint64_t len) -> uint64_t;

  /**
   * @brief 返回数据的长度
   */
  auto GetSize() const -> size_t { return size_; }

 private:
  /**
   * @brief 扩容ByteArray,使其可以容纳size个数据(如果原本可以可以容纳,则不扩容)
   */
  void AddCapacity(size_t size);

  /**
   * @brief 获取当前的可写入容量
   */
  auto GetRemainCapacity() const -> size_t { return capacity_ - total_cur_pos_; }

 private:
  /// 内存块的大小，单位是byte
  size_t node_size_{0};
  /// 总的当前操作位置
  size_t total_cur_pos_{0};
  /// 已申请的总容量，所有node的容量之和，单位是byte
  size_t capacity_{0};
  /// 当前数据的长度，单位是byte
  size_t size_{0};
  /// 字节序,默认大端
  int8_t endian_{WTSCLWQ_BIG_ENDIAN};
  /// 第一个内存块指针
  Node *root_{};
  /// 当前操作的内存块指针
  Node *cur_node_{};
};

}  // namespace wtsclwq

#endif  // _WTSCLWQ_SERIALIZE_