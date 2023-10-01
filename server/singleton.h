#ifndef _WTSCLWQ_SINGLETON_
#define _WTSCLWQ_SINGLETON_

#include <memory>
namespace wtsclwq {
template <typename T, typename X, int N>
auto GetInstance() -> T & {
  static T instance;
  return instance;
}

template <typename T, typename X, int N>
auto GetInstancePtr() -> std::shared_ptr<T> {
  static std::shared_ptr<T> instance(new T);
  return instance;
}

/**
 * @brief 单例模式的裸指针版本
 *
 * @tparam T 目标类型
 * @tparam X 多个实例不同的Tag
 * @tparam N 同一个Tag下的多个实例的编号
 */
template <typename T, typename X = void, int N = 0>
class Singleton {
 public:
  static auto GetInstance() -> T * {
    static T instance;
    return &instance;
  }
};

/**
 * @brief 单例模式的智能指针版本
 *
 * @tparam T 目标类型
 * @tparam X 多个实例不同的Tag
 * @tparam N 同一个Tag下的多个实例的编号
 */
template <typename T, typename X = void, int N = 0>
class SingletonPtr {
 public:
  static auto GetInstance() -> std::shared_ptr<T> {
    static std::shared_ptr<T> instance(new T);
    return instance;
  }
};

}  // namespace wtsclwq
#endif