#ifndef _WTSCLWQ_STREAM_
#define _WTSCLWQ_STREAM_

#include <memory>
#include "serialize.h"

namespace wtsclwq {

/**
 * @brief 流结构
 */
class Stream {
 public:
  using s_ptr = std::shared_ptr<Stream>;
  /**
   * @brief 析构函数
   */
  virtual ~Stream();

  /**
   * @brief 读数据
   * @param[out] buffer 接收数据的内存
   * @param[in] length 接收数据的内存大小
   * @return
   *      @retval >0 返回接收到的数据的实际大小
   *      @retval =0 被关闭
   *      @retval <0 出现流错误
   */
  virtual auto Read(void *buffer, size_t length) -> int = 0;

  /**
   * @brief 读数据
   * @param[out] ba 接收数据的ByteArray
   * @param[in] length 接收数据的内存大小
   * @return
   *      @retval >0 返回接收到的数据的实际大小
   *      @retval =0 被关闭
   *      @retval <0 出现流错误
   */
  virtual auto ReadToByteArray(ByteArray::s_ptr ba, size_t length) -> int = 0;

  /**
   * @brief 读固定长度的数据
   * @param[out] buffer 接收数据的内存
   * @param[in] length 接收数据的内存大小
   * @return
   *      @retval >0 返回接收到的数据的实际大小
   *      @retval =0 被关闭
   *      @retval <0 出现流错误
   */
  virtual auto ReadFixSize(void *buffer, size_t length) -> int;

  /**
   * @brief 读固定长度的数据
   * @param[out] ba 接收数据的ByteArray
   * @param[in] length 接收数据的内存大小
   * @return
   *      @retval >0 返回接收到的数据的实际大小
   *      @retval =0 被关闭
   *      @retval <0 出现流错误
   */
  virtual auto ReadFixSizeToByteArray(const ByteArray::s_ptr &ba, size_t length) -> int;

  /**
   * @brief 写数据
   * @param[in] buffer 写数据的内存
   * @param[in] length 写入数据的内存大小
   * @return
   *      @retval >0 返回写入到的数据的实际大小
   *      @retval =0 被关闭
   *      @retval <0 出现流错误
   */
  virtual auto Write(const void *buffer, size_t length) -> int = 0;

  /**
   * @brief 写数据
   * @param[in] ba 写数据的ByteArray
   * @param[in] length 写入数据的内存大小
   * @return
   *      @retval >0 返回写入到的数据的实际大小
   *      @retval =0 被关闭
   *      @retval <0 出现流错误
   */
  virtual auto WriteToByteArray(const ByteArray::s_ptr &ba, size_t length) -> int = 0;

  /**
   * @brief 写固定长度的数据
   * @param[in] buffer 写数据的内存
   * @param[in] length 写入数据的内存大小
   * @return
   *      @retval >0 返回写入到的数据的实际大小
   *      @retval =0 被关闭
   *      @retval <0 出现流错误
   */
  virtual auto WriteFixSize(const void *buffer, size_t length) -> int;

  /**
   * @brief 写固定长度的数据
   * @param[in] ba 写数据的ByteArray
   * @param[in] length 写入数据的内存大小
   * @return
   *      @retval >0 返回写入到的数据的实际大小
   *      @retval =0 被关闭
   *      @retval <0 出现流错误
   */
  virtual auto WriteFixSizeToByteArray(const ByteArray::s_ptr &ba, size_t length) -> int;

  /**
   * @brief 关闭流
   */
  virtual void Close() = 0;
};
}  // namespace wtsclwq
#endif  // _WTSCLWQ_STREAM_