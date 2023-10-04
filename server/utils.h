#ifndef _WTSCLWQ_UTILS_
#define _WTSCLWQ_UTILS_

#include <cxxabi.h>
#include <sys/types.h>
#include <ios>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace wtsclwq {
/**
 * @brief 获取当前线程的pthread id
 *
 * @return pid_t
 */
auto GetCurrSysThreadId() -> pid_t;

/**
 * @brief 获取当前协程的id
 */
auto GetCurrCouroutineId() -> uint64_t;

/**
 * @brief 获取当前线程的名称
 */
auto GetCurrSysThreadName() -> std::string;

/**
 * @brief 设置当前线程的名称
 */
void SetCurrSysThreadName(std::string_view name);

/**
 * @brief 获取当前调用栈
 * @param bt 保存调用栈的字符串数组
 * @param size 最多保存多少层调用栈
 * @param skip 跳过多少层调用栈
 */
void Backtrace(std::vector<std::string> *bt, int size = 64, int skip = 1);

/**
 * @brief 获取当前栈帧的信息
 * @param[out] bt 保存调用栈的字符串数组
 * @param[in] size 最多保存多少层调用栈
 * @param[in] skip 跳过多少层调用栈
 */
auto BacktraceToString(int size = 64, int skip = 2, std::string_view prefix = "") -> std::string;

/**
 * @brief 获取当前时间的毫秒数
 */
auto GetCurrMs() -> uint64_t;

/**
 * @brief 获取当前时间的微秒数
 */
auto GetCurrUs() -> uint64_t;

/**
 * @brief 获取当前距离服务器启动的时间毫秒数
 *
 * @return int64_t
 */
auto GetElapsedTime() -> int64_t;

/**
 * @brief 全部转大写
 */
auto ToUpper(std::string_view str) -> std::string;

/**
 * @brief 全部转小写
 */
auto ToLower(std::string_view str) -> std::string;

/**
 * @brief 字符串转日期+时间
 */
auto StrToTime(std::string_view str, std::string_view format = "%Y-%m-%d %H:%M:%S") -> time_t;

/**
 * @brief 日期+时间转字符串
 */
auto TimeToStr(time_t ts, std::string_view format = "%Y-%m-%d %H:%M:%S") -> std::string;

/**
 * @brief 获取T类型的类型字符串
 */
template <class T>
auto TypeToName() -> const char * {
  static const char *s_name = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, nullptr);
  return s_name;
}
/**
 * @brief 文件系统操作类
 */
class FSUtil {
 public:
  /**
   * @brief 递归列举指定目录下所有指定后缀的常规文件，如果不指定后缀，则遍历所有文件，返回的文件名带路径
   * @param[out] files 文件列表
   * @param[in] path 路径
   * @param[in] subfix 后缀名，比如 ".yml"
   */
  static void ListAllFile(std::vector<std::string> *files, std::string_view path, std::string_view subfix);

  /**
   * @brief 创建路径，相当于mkdir -p
   * @param[in] dirname 路径名
   * @return 创建是否成功
   */
  static auto Mkdir(std::string_view dirname) -> bool;

  /**
   * @brief 判断指定pid文件指定的pid是否正在运行，使用kill(pid, 0)的方式判断
   * @param[in] pidfile 保存进程号的文件
   * @return 是否正在运行
   */
  static auto IsRunningPidfile(std::string_view pidfile) -> bool;

  /**
   * @brief 删除文件或路径
   * @param[in] path 文件名或路径名
   * @return 是否删除成功
   */
  static auto Rm(std::string_view path) -> bool;

  /**
   * @brief 移动文件或路径，内部实现是先Rm(to)，再rename(from, to)，参考rename
   * @param[in] from 源
   * @param[in] to 目的地
   * @return 是否成功
   */
  static auto Mv(std::string_view from, std::string_view to) -> bool;

  /**
   * @brief 返回绝对路径，参考realpath(3)
   * @details 路径中的符号链接会被解析成实际的路径，删除多余的'.' '..'和'/'
   * @param[in] path
   * @param[out] rpath
   * @return  是否成功
   */
  static auto Realpath(std::string_view path, std::string *rpath) -> bool;

  /**
   * @brief 创建符号链接，参考symlink(2)
   * @param[in] from 目标
   * @param[in] to 链接路径
   * @return  是否成功
   */
  static auto Symlink(std::string_view from, std::string_view to) -> bool;

  /**
   * @brief 删除文件，参考unlink(2)
   * @param[in] filename 文件名
   * @param[in] exist 是否存在
   * @return  是否成功
   * @note 内部会判断一次是否真的不存在该文件
   */
  static auto Unlink(std::string_view filename, bool exist = false) -> bool;

  /**
   * @brief 返回文件，即路径中最后一个/前面的部分，不包括/本身，如果未找到，则返回filename
   * @param[in] filename 文件完整路径
   * @return  文件路径
   */
  static auto Dirname(std::string_view filename) -> std::string;

  /**
   * @brief 返回文件名，即路径中最后一个/后面的部分
   * @param[in] filename 文件完整路径
   * @return  文件名
   */
  static auto Basename(std::string_view filename) -> std::string;

  /**
   * @brief 以只读方式打开
   * @param[in] ifs 文件流
   * @param[in] filename 文件名
   * @param[in] mode 打开方式
   * @return  是否打开成功
   */
  static auto OpenForRead(std::ifstream &ifs, const std::string &filename, std::ios_base::openmode mode) -> bool;

  /**
   * @brief 以只写方式打开
   * @param[in] ofs 文件流
   * @param[in] filename 文件名
   * @param[in] mode 打开方式
   * @return  是否打开成功
   */
  static auto OpenForWrite(std::ofstream &ofs, const std::string &filename, std::ios_base::openmode mode) -> bool;
};

/**
 * @brief 字符串操作
 */
class StringUtil {
 public:
  /**
   * @brief printf风格的字符串格式化，返回格式化后的string
   */
  static auto Format(const char *fmt, ...) -> std::string;

  /**
   * @brief vprintf风格的字符串格式化，返回格式化后的string
   */
  static auto Formatv(const char *fmt, va_list ap) -> std::string;

  /**
   * @brief url编码
   * @param[in] str 原始字符串
   * @param[in] space_as_plus 是否将空格编码成+号，如果为false，则空格编码成%20
   * @return 编码后的字符串
   */
  static auto UrlEncode(std::string_view str, bool space_as_plus = true) -> std::string;

  /**
   * @brief url解码
   * @param[in] str url字符串
   * @param[in] space_as_plus 是否将+号解码为空格
   * @return 解析后的字符串
   */
  static auto UrlDecode(std::string_view str, bool space_as_plus = true) -> std::string;

  /**
   * @brief 移除字符串首尾的指定字符串
   * @param[] str 输入字符串
   * @param[] delimit 待移除的字符串
   * @return  移除后的字符串
   */
  static auto Trim(std::string_view str, std::string_view delimit = " \t\r\n") -> std::string;

  /**
   * @brief 移除字符串首部的指定字符串
   * @param[] str 输入字符串
   * @param[] delimit 待移除的字符串
   * @return  移除后的字符串
   */
  static auto TrimLeft(std::string_view, std::string_view delimit = " \t\r\n") -> std::string;

  /**
   * @brief 移除字符尾部的指定字符串
   * @param[] str 输入字符串
   * @param[] delimit 待移除的字符串
   * @return  移除后的字符串
   */
  static auto TrimRight(std::string_view str, std::string_view delimit = " \t\r\n") -> std::string;

  /**
   * @brief 宽字符串转字符串
   */
  static auto WStringToString(const std::wstring &ws) -> std::string;

  /**
   * @brief 字符串转宽字符串
   */
  static auto StringToWString(std::string_view s) -> std::wstring;
};

}  // namespace wtsclwq

#endif  // _WTSCLWQ_UTILS_