
#ifndef __WTSCLWQ_ENDIAN_H__
#define __WTSCLWQ_ENDIAN_H__

#define WTSCLWQ_LITTLE_ENDIAN 1
#define WTSCLWQ_BIG_ENDIAN 2

#include <byteswap.h>
#include <cstdint>
#include <type_traits>

namespace wtsclwq {

/**
 * @brief 8字节类型的字节序转化
 */
template <class T>
auto Byteswap(T value) -> typename std::enable_if<sizeof(T) == sizeof(uint64_t), T>::type {
  return static_cast<T>(bswap_64((uint64_t)value));
}

/**
 * @brief 4字节类型的字节序转化
 */
template <class T>
auto Byteswap(T value) -> typename std::enable_if<sizeof(T) == sizeof(uint32_t), T>::type {
  return static_cast<T>(bswap_32((uint32_t)value));
}

/**
 * @brief 2字节类型的字节序转化
 */
template <class T>
auto Byteswap(T value) -> typename std::enable_if<sizeof(T) == sizeof(uint16_t), T>::type {
  return static_cast<T>(bswap_16((uint16_t)value));
}

#if BYTE_ORDER == BIG_ENDIAN
#define WTSCLWQ_BYTE_ORDER WTSCLWQ_BIG_ENDIAN
#else
#define WTSCLWQ_BYTE_ORDER WTSCLWQ_LITTLE_ENDIAN
#endif

#if WTSCLWQ_BYTE_ORDER == WTSCLWQ_BIG_ENDIAN

/**
 * @brief 只在小端机器上执行byteswap, 在大端机器上什么都不做
 */
template <class T>
auto OnlyByteswapOnLittleEndian(T t) -> T {
  return t;
}

/**
 * @brief 只在大端机器上执行byteswap, 在小端机器上什么都不做
 */
template <class T>
auto OnlyByteswapOnBigEndian(T t) -> T {
  return Byteswap(t);
}
#else

/**
 * @brief 只在小端机器上执行byteswap, 在大端机器上什么都不做
 */
template <class T>
auto OnlyByteswapOnLittleEndian(T t) -> T {
  return Byteswap(t);
}

/**
 * @brief 只在大端机器上执行byteswap, 在小端机器上什么都不做
 */
template <class T>
auto OnlyByteswapOnBigEndian(T t) -> T {
  return t;
}
#endif

}  // namespace wtsclwq

#endif
