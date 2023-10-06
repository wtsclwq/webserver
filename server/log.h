#ifndef _WTSCLWQ_LOG_
#define _WTSCLWQ_LOG_

#include <bits/types/time_t.h>
#include <cstdint>
#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <ostream>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "lock.h"
#include "singleton.h"
#include "utils.h"

#define ROOT_LOGGER wtsclwq::LoggerMgr::GetInstance()->GetRoot()

#define NAMED_LOGGER(name) wtsclwq::LoggerMgr::GetInstance()->GetLogger(name)

#define LEVELED_LOG(logger, level)                                                                                    \
  if ((level) >= (logger)->GetLevel())                                                                                \
  wtsclwq::LogEventWrap(logger, std::make_shared<wtsclwq::LogEvent>(                                                  \
                                    level, __FILE__, __LINE__, wtsclwq::GetElapsedTime() - (logger)->GetCreateTime(), \
                                    wtsclwq::GetCurrSysThreadId(), wtsclwq::GetCurrCouroutineId(),                    \
                                    wtsclwq::GetCurrSysThreadName(), (logger)->GetName(), time(0)))                   \
      .GetEvent()                                                                                                     \
      ->GetStream()

#define LOG_DEBUG(logger) LEVELED_LOG(logger, wtsclwq::LogLevel::DEBUG)

#define LOG_INFO(logger) LEVELED_LOG(logger, wtsclwq::LogLevel::INFO)

#define LOG_NOTICE(logger) LEVELED_LOG(logger, wtsclwq::LogLevel::NOTICE)

#define LOG_WARN(logger) LEVELED_LOG(logger, wtsclwq::LogLevel::WARN)

#define LOG_ERROR(logger) LEVELED_LOG(logger, wtsclwq::LogLevel::ERROR)

#define LOG_CRIT(logger) LEVELED_LOG(logger, wtsclwq::LogLevel::CRIT)

#define LOG_ALERT(logger) LEVELED_LOG(logger, wtsclwq::LogLevel::ALERT)

#define LOG_FATAL(logger) LEVELED_LOG(logger, wtsclwq::LogLevel::FATAL)

#define FMT_LEVELED_LOG(logger, level, fmt, ...)                                                                    \
  if ((level) >= (logger)->GetLevel())                                                                              \
  wtsclwq::LogEventWrap(                                                                                            \
      (logger), std::make_shared<wtsclwq::LogEvent>(level, __FILE__, __LINE__,                                      \
                                                    wtsclwq::GetElapsedTime() - (logger)->GetCreateTime(),          \
                                                    wtsclwq::GetCurrSysThreadId(), wtsclwq::GetCurrCouroutineId(),  \
                                                    wtsclwq::GetCurrSysThreadName(), (logger)->GetName(), time(0))) \
      .GetEvent()                                                                                                   \
      ->Printf((fmt), ##__VA_ARGS__)

#define FMT_LOG_DEBUG(logger, fmt, ...) FMT_LEVELED_LOG(logger, wtsclwq::LogLevel::DEBUG, fmt, ##__VA_ARGS__)

#define FMT_LOG_INFO(logger, fmt, ...) FMT_LEVELED_LOG(logger, wtsclwq::LogLevel::INFO, fmt, ##__VA_ARGS__)

#define FMT_LOG_NOTICE(logger, fmt, ...) FMT_LEVELED_LOG(logger, wtsclwq::LogLevel::NOTICE, fmt, ##__VA_ARGS__)

#define FMT_LOG_WARN(logger, fmt, ...) FMT_LEVELED_LOG(logger, wtsclwq::LogLevel::WARN, fmt, ##__VA_ARGS__)

#define FMT_LOG_ERROR(logger, fmt, ...) FMT_LEVELED_LOG(logger, wtsclwq::LogLevel::ERROR, fmt, ##__VA_ARGS__)

#define FMT_LOG_CRIT(logger, fmt, ...) FMT_LEVELED_LOG(logger, wtsclwq::LogLevel::CRIT, fmt, ##__VA_ARGS__)

#define FMT_LOG_ALERT(logger, fmt, ...) FMT_LEVELED_LOG(logger, wtsclwq::LogLevel::ALERT, fmt, ##__VA_ARGS__)

#define FMT_LOG_FATAL(logger, fmt, ...) FMT_LEVELED_LOG(logger, wtsclwq::LogLevel::FATAL, fmt, ##__VA_ARGS__)

namespace wtsclwq {

enum class LogLevel {
  UNKNOWN = 0,   // 未知
  INFO = 100,    // 信息
  NOTICE = 200,  // 通知
  DEBUG = 300,   // 调试
  WARN = 400,    // 警告
  ERROR = 500,   // 业务错误
  CRIT = 600,    // 硬件错误
  ALERT = 700,   // 外部依赖崩溃
  FATAL = 800,   // 致命
};
/**
 * @brief 日志级别转字符串
 *
 * @param level
 * @return const char* c风格字符串，用作打印和写入文件
 */
static auto ToString(LogLevel level) -> const char *;

/**
 * @brief 字符串转日志级别
 *
 * @param str
 * @return LogLevel
 */
static auto FromString(std::string_view str) -> LogLevel;

class LogEvent {
 public:
  using s_ptr = std::shared_ptr<LogEvent>;

  LogEvent(LogLevel level, const char *file, int32_t line, int64_t elapse, uint32_t thread_id, uint64_t coroutine_id,
           std::string_view thread_name, std::string_view logger_name, time_t time);

  ~LogEvent() = default;

  auto GetLevel() const -> LogLevel { return level_; }

  auto GetContent() const -> std::string { return ss_.str(); }

  auto GetElapse() const -> int64_t { return elapse_; }

  auto GetThreadId() const -> uint32_t { return thread_id_; }

  auto GetCoroutineId() const -> uint64_t { return coroutine_id_; }

  auto GetThreadName() const -> std::string { return thread_name_; }

  auto GetLoggerName() const -> std::string { return logger_name_; }

  auto GetFile() const -> std::string { return file_; }

  auto GetLine() const -> int { return line_; }

  auto GetTime() const -> time_t { return time_; }

  /**
   * @brief 获取字符流，用于流式写入日志
   *
   * @return std::stringstream&
   */
  auto GetStream() -> std::stringstream & { return ss_; }

  /**
   * @brief C风格，printf格式化写入日志
   *
   * @param fmt
   * @param ...
   */
  void Printf(const char *fmt, ...);

 private:
  LogLevel level_{0};          // 事件级别
  std::stringstream ss_{};     // 字符流，存储日志内存
  std::string file_{};         // 用文件输出时的文件名
  int line_{0};                // 日志事件发生处的行号
  int64_t elapse_{0};          // 程序启动(日志器创建）到现在的毫秒数
  uint32_t thread_id_{0};      // 日志事件发生的线程id
  uint64_t coroutine_id_{0};   // 日志事件发生的协程id
  std::string thread_name_{};  // 日志事件发生的线程名称
  std::string logger_name_{};  // 日志器名称
  time_t time_{};              // 日志发生的时间戳
};

class LogFormatter {
 public:
  using s_ptr = std::shared_ptr<LogFormatter>;

  /**
   * @brief 构造函数
   * @param pattern 格式模板，参考sylar与log4cpp
   * @details 模板参数说明：
   * - %%m 消息
   * - %%p 日志级别
   * - %%c 日志器名称
   * - %%d 日期时间，后面可跟一对括号指定时间格式，比如%%d{%%Y-%%m-%%d %%H:%%M:%%S}，这里的格式字符与C语言strftime一致
   * - %%r 该日志器创建后的累计运行毫秒数
   * - %%f 文件名
   * - %%l 行号
   * - %%t 线程id
   * - %%C 协程id
   * - %%N 线程名称
   * - %%% 百分号
   * - %%T 制表符
   * - %%n 换行
   *
   * 默认格式：%%d{%%Y-%%m-%%d %%H:%%M:%%S}%%T%%t%%T%%N%%T%%C%%T[%%p]%%T[%%c]%%T%%f:%%l%%T%%m%%n
   *
   * 默认格式描述：年-月-日 时:分:秒 [累计运行毫秒数] \\t 线程id \\t 线程名称 \\t 协程id \\t [日志级别] \\t [日志器名称]
   * \\t 文件名:行号 \\t 日志消息 换行符
   */
  explicit LogFormatter(std::string_view pattern = "%d{%Y-%m-%d %H:%M:%S} [%rms] %t %N %C [%p] [%c] %f:%l %m%n");

  /**
   * @brief 初始化，解析日志格式模板
   *
   */
  void Init();

  /**
   * @brief 模板解析是否出现错误
   *
   * @return true
   * @return false
   */
  auto HasError() const -> bool { return has_error_; }

  auto GetPattern() const -> std::string { return pattern_; }

  /**
   * @brief 对日志事件，按照模板格式化并返回格式化后的字符串文本
   *
   * @param event 要格式化的日志事件
   * @return std::string
   */
  auto Format(const LogEvent::s_ptr &event) -> std::string;

  /**
   * @brief 对日志事件，按照模板格式化并写入到流中，并返回流
   *
   * @param event 要格式化的日志事件
   * @param os 要写入的流
   * @return std::string& 返回流
   */
  auto Format(const LogEvent::s_ptr &event, std::ostream &os) -> std::ostream &;

  /**
   * @brief 日志格式项基类，通过不同的派生类实现对日志事件不同字段的格式化
   */
  class FormatItem {
   public:
    using s_ptr = std::shared_ptr<FormatItem>;

    virtual ~FormatItem() = default;

    virtual void Format(std::ostream &os, LogEvent::s_ptr event) = 0;
  };

 private:
  std::string pattern_{};                   // 日志格式模板
  std::vector<FormatItem::s_ptr> items_{};  // 日志格式模板解析后的日志格式项
  bool has_error_{false};                   // 解析过程是否出现错误
};

class LogAppender {
 public:
  using MutexType = SpinLock;
  using s_ptr = std::shared_ptr<LogAppender>;

  explicit LogAppender(LogFormatter::s_ptr formatter);

  virtual ~LogAppender() = default;

  void SetFormatter(LogFormatter::s_ptr formatter);

  auto GetFormatter() const -> LogFormatter::s_ptr;

  virtual void Log(LogEvent::s_ptr event) = 0;

  virtual auto FlushConfigToYmal() -> std::string = 0;

 protected:
  LogFormatter::s_ptr formatter_{};          // 日志格式器
  LogFormatter::s_ptr default_formatter_{};  // 默认日志格式器
  mutable MutexType mutex_{};                // 互斥锁
};

class StdoutLogAppender : public LogAppender {
 public:
  using s_ptr = std::shared_ptr<StdoutLogAppender>;

  StdoutLogAppender();

  ~StdoutLogAppender() override = default;

  void Log(LogEvent::s_ptr event) override;

  auto FlushConfigToYmal() -> std::string override;
};

class FileLogAppender : public LogAppender {
 public:
  using s_ptr = std::shared_ptr<FileLogAppender>;

  explicit FileLogAppender(std::string_view filename);

  ~FileLogAppender() override = default;

  void Log(LogEvent::s_ptr event) override;

  auto FlushConfigToYmal() -> std::string override;

  auto Reopen() -> bool;

 private:
  std::string filename_{};       // 日志文件名
  std::ofstream file_stream_{};  // 日志文件流
  uint64_t last_time_{0};        // 上次打开文件的时间
  bool open_error_{false};       // 打开文件是否出错
};

class Logger {
 public:
  using MutexType = SpinLock;
  using s_ptr = std::shared_ptr<Logger>;

  explicit Logger(std::string_view name = "default");

  ~Logger() = default;

  auto GetName() const -> std::string;

  auto GetLevel() const -> LogLevel;

  auto GetCreateTime() const -> time_t;

  void SetLevel(LogLevel level);

  /**
   * @brief 添加一个日志输出目标
   *
   * @param appender
   */
  void AddAppender(LogAppender::s_ptr appender);

  /**
   * @brief 移除一个日志输出目标
   *
   * @param appender
   */
  void RemoveAppender(const LogAppender::s_ptr &appender);

  /**
   * @brief 清空日志器的日志输出目标集合
   *
   */
  void ClearAppenders();

  /**
   * @brief 记录日志事件
   *
   * @param event
   */
  void Log(const LogEvent::s_ptr &event);

  /**
   * @brief 将日期器的配置转换为yaml格式的字符串
   *
   * @return std::string
   */
  auto FlushConfigToYmal() -> std::string;

 private:
  std::string name_{};                         // 日志器名称
  LogLevel level_{LogLevel::DEBUG};            // 日志器级别
  std::list<LogAppender::s_ptr> appenders_{};  // 日志器的日志输出目标集合
  time_t create_time_;                         // 日志器创建时间
  mutable MutexType mutex_{};                  // 互斥锁
};

/**
 * @brief 日志时间包装器，将日志事件和日志器绑定，方便日志宏使用
 */
class LogEventWrap {
 public:
  LogEventWrap(Logger::s_ptr logger, LogEvent::s_ptr event);

  ~LogEventWrap();

  auto GetEvent() const -> LogEvent::s_ptr { return event_; }

 private:
  Logger::s_ptr logger_;   // 日志器
  LogEvent::s_ptr event_;  // 日志事件
};

class LoggerManager {
 public:
  using MutexType = SpinLock;

  LoggerManager();

  ~LoggerManager() = default;

  /**
   * @brief 如果指定名称的日志器未找到，那会就新创建一个，
   * 但是新创建的Logger是不带Appender的，需要手动添加Appender
   * @param name
   * @return Logger::s_ptr
   */
  auto GetLogger(const std::string &name) -> Logger::s_ptr;

  auto CreateEmptyLogger(const std::string &name) -> Logger::s_ptr;

  auto GetRoot() const -> Logger::s_ptr;

  auto FlushConfigToYmal() -> std::string;

 private:
  std::unordered_map<std::string, Logger::s_ptr> loggers_{};  // 日志器集合
  Logger::s_ptr root_logger_{};                               // 根日志器
  mutable MutexType mutex_{};                                 // 互斥锁
};

using LoggerMgr = SingletonPtr<LoggerManager>;

}  // namespace wtsclwq
#endif