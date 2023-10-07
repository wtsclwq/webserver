#ifndef _WTSCLWQ_SOCKET_STREAM_
#define _WTSCLWQ_SOCKET_STREAM_

#include "server/address.h"
#include "server/socket.h"
#include "server/stream.h"

namespace wtsclwq {
class SocketStream : public Stream {
 public:
  using s_ptr = std::shared_ptr<SocketStream>;

  /**
   * @brief 构造函数
   * @param socket 该Stream进行通信使用的Socket
   * @param owner 是否拥有该Socket
   */
  explicit SocketStream(SocketWrap::s_ptr socket, bool is_owner = true);

  ~SocketStream() override;

  /**
   * @brief 从Socket中读取数据
   * @param buffer 存放从Socket中读取的数据的内存
   * @param length 读取数据的长度
   * @return 读取的数据长度，如果返回值小于0，表示读取失败
   */
  auto Read(void *buffer, size_t length) -> int override;

  /**
   * @brief 从Socket中读取数据到ByteArray中
   * @param ba 存放从Socket中读取的数据的ByteArray
   * @param length 读取数据的长度
   * @return 读取的数据长度，如果返回值小于0，表示读取失败
   */
  auto ReadToByteArray(const ByteArray::s_ptr &ba, size_t length) -> int override;

  /**
   * @brief 向Socket中写入数据
   * @param buffer 待写入Socket的数据的内存
   * @param length 写入数据的长度
   * @return 写入的数据长度，如果返回值小于0，表示写入失败
   */
  auto Write(const void *buffer, size_t length) -> int override;

  /**
   * @brief 向Socket中写入ByteArray中的数据
   * @param ba 待写入Socket的数据的ByteArray
   * @param length 写入数据的长度
   * @return 写入的数据长度，如果返回值小于0，表示写入失败
   */
  auto WriteFromByteArray(const ByteArray::s_ptr &ba, size_t length) -> int override;

  /**
   * @brief 关闭Socket
   */
  void Close() override;

  /**
   * @brief 获取Socket::s_ptr
   */
  auto GetSocket() -> SocketWrap::s_ptr;

  /**
   * @brief SocketStream是否连通
   */
  auto IsConnected() -> bool;

  auto GetLocalAddress() -> Address::s_ptr;

  auto GetRemoteAddress() -> Address::s_ptr;

  auto GetLocalAddressString() -> std::string;

  auto GetRemoteAddressString() -> std::string;

 protected:
  SocketWrap::s_ptr socket_;
  bool is_owner_;
};
}  // namespace wtsclwq

#endif  // _WTSCLWQ_SOCKET_STREAM_