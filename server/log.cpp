#include "log.h"
#include <yaml-cpp/node/node.h>
#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "config.h"
#include "env.h"
#include "utils.h"

namespace wtsclwq {
static auto ToString(LogLevel level) -> const char * {
  switch (level) {
    case LogLevel::UNKNOWN:
      return "UNKNOWN";
    case LogLevel::INFO:
      return "INFO";
    case LogLevel::NOTICE:
      return "NOTICE";
    case LogLevel::DEBUG:
      return "DEBUG";
    case LogLevel::WARN:
      return "WARN";
    case LogLevel::ERROR:
      return "ERROR";
    case LogLevel::CRIT:
      return "CRIT";
    case LogLevel::ALERT:
      return "ALERT";
    case LogLevel::FATAL:
      return "FATAL";
    default:
      return "UNKNOWN";
  }
}

static auto FromString(std::string_view str) -> LogLevel {
  if (str == "INFO" || str == "info") {
    return LogLevel::INFO;
  }
  if (str == "NOTICE" || str == "notice") {
    return LogLevel::NOTICE;
  }
  if (str == "DEBUG" || str == "debug") {
    return LogLevel::DEBUG;
  }
  if (str == "WARN" || str == "warn") {
    return LogLevel::WARN;
  }
  if (str == "ERROR" || str == "error") {
    return LogLevel::ERROR;
  }
  if (str == "CRIT" || str == "crit") {
    return LogLevel::CRIT;
  }
  if (str == "ALERT" || str == "alert") {
    return LogLevel::ALERT;
  }
  if (str == "FATAL" || str == "fatal") {
    return LogLevel::FATAL;
  }
  return LogLevel::UNKNOWN;
}

LogEvent::LogEvent(LogLevel level, const char *file, int32_t line, int64_t elapse, uint32_t thread_id,
                   uint64_t coroutine_id, std::string_view thread_name, std::string_view logger_name, time_t time)
    : level_(level),
      file_(file),
      line_(line),
      elapse_(elapse),
      thread_id_(thread_id),
      coroutine_id_(coroutine_id),
      thread_name_(thread_name),
      logger_name_(logger_name),  // Fixed order of initialization
      time_(time) {}

void LogEvent::Printf(const char *fmt...) {
  va_list args;
  va_start(args, fmt);
  auto formatted_string = StringUtil::Formatv(fmt, args);
  va_end(args);

  ss_ << formatted_string;
}

class MessageFormatItem : public LogFormatter::FormatItem {
 public:
  explicit MessageFormatItem(std::string_view str) {}

  void Format(std::ostream &os, LogEvent::s_ptr event) override { os << event->GetContent(); }
};

class LevelFormatItem : public LogFormatter::FormatItem {
 public:
  explicit LevelFormatItem(std::string_view str) {}

  void Format(std::ostream &os, LogEvent::s_ptr event) override { os << ToString(event->GetLevel()); }
};

class ElapseFormatItem : public LogFormatter::FormatItem {
 public:
  explicit ElapseFormatItem(std::string_view str) {}

  void Format(std::ostream &os, LogEvent::s_ptr event) override { os << event->GetElapse(); }
};

class ThreadIdFormatItem : public LogFormatter::FormatItem {
 public:
  explicit ThreadIdFormatItem(std::string_view str) {}

  void Format(std::ostream &os, LogEvent::s_ptr event) override { os << event->GetThreadId(); }
};

class CoroutineIdFormatItem : public LogFormatter::FormatItem {
 public:
  explicit CoroutineIdFormatItem(std::string_view str) {}

  void Format(std::ostream &os, LogEvent::s_ptr event) override { os << event->GetCoroutineId(); }
};

class ThreadNameFormatItem : public LogFormatter::FormatItem {
 public:
  explicit ThreadNameFormatItem(std::string_view str) {}

  void Format(std::ostream &os, LogEvent::s_ptr event) override { os << event->GetThreadName(); }
};

class DateTimeFormatItem : public LogFormatter::FormatItem {
 public:
  explicit DateTimeFormatItem(std::string_view format = "%Y-%m-%d %H:%M:%S") : format_(format) {
    if (format.empty()) {
      format_ = "%Y-%m-%d %H:%M:%S";
    }
  }

  void Format(std::ostream &os, LogEvent::s_ptr event) override {
    struct tm tm;
    time_t time = event->GetTime();
    localtime_r(&time, &tm);
    char buf[64];
    strftime(buf, sizeof(buf), format_.c_str(), &tm);
    os << buf;
  }

 private:
  std::string format_{};
};

class FilenameFormatItem : public LogFormatter::FormatItem {
 public:
  explicit FilenameFormatItem(std::string_view str) {}

  void Format(std::ostream &os, LogEvent::s_ptr event) override { os << event->GetFile(); }
};

class LineFormatItem : public LogFormatter::FormatItem {
 public:
  explicit LineFormatItem(std::string_view str) {}

  void Format(std::ostream &os, LogEvent::s_ptr event) override { os << event->GetLine(); }
};

class NewLineFormatItem : public LogFormatter::FormatItem {
 public:
  explicit NewLineFormatItem(std::string_view str) {}

  void Format(std::ostream &os, LogEvent::s_ptr event) override { os << std::endl; }
};

class StringFormatItem : public LogFormatter::FormatItem {
 public:
  explicit StringFormatItem(std::string_view str) : str_(str) {}

  void Format(std::ostream &os, LogEvent::s_ptr event) override { os << str_; }

 private:
  std::string str_;
};

class TabFormatItem : public LogFormatter::FormatItem {
 public:
  explicit TabFormatItem(std::string_view str) {}

  void Format(std::ostream &os, LogEvent::s_ptr event) override { os << "\t"; }
};

class PercentFormatItem : public LogFormatter::FormatItem {
 public:
  explicit PercentFormatItem(std::string_view str) {}

  void Format(std::ostream &os, LogEvent::s_ptr event) override { os << "%"; }
};

class LoggerNameFormatItem : public LogFormatter::FormatItem {
 public:
  explicit LoggerNameFormatItem(std::string_view str) {}

  void Format(std::ostream &os, LogEvent::s_ptr event) override { os << event->GetLoggerName(); }
};

LogFormatter::LogFormatter(std::string_view pattern) : pattern_(pattern) { Init(); }

void LogFormatter::Init() {
  // 按顺序存储解析到的pattern项
  // 每个pattern包括一个整数类型和一个字符串，类型为0表示该pattern是常规字符串，为1表示该pattern需要转义
  // 日期格式单独用下面的dataformat存储
  std::vector<std::pair<int, std::string>> patterns;
  // 临时存储常规字符串
  std::string tmp;
  // 日期格式字符串，默认把位于%d后面的大括号对里的全部字符都当作格式字符，不校验格式是否合法
  std::string dateformat;
  // 是否解析出错
  bool error = false;

  // 是否正在解析常规字符，初始时为true
  bool parsing_string = true;
  // 是否正在解析模板字符，%后面的是模板字符
  // bool parsing_pattern = false;

  size_t i = 0;
  while (i < pattern_.size()) {
    std::string c = std::string(1, pattern_[i]);
    if (c == "%") {
      if (parsing_string) {
        if (!tmp.empty()) {
          patterns.emplace_back(0, tmp);
        }
        tmp.clear();
        parsing_string = false;  // 在解析常规字符时遇到%，表示开始解析模板字符
        i++;
        continue;
      }
      patterns.emplace_back(1, c);
      parsing_string = true;  // 在解析模板字符时遇到%，表示这里是一个%转义
      i++;
      continue;
    }
    // not %
    if (parsing_string) {  // 持续解析常规字符直到遇到%，解析出的字符串作为一个常规字符串加入patterns
      tmp += c;
      i++;
      continue;
    }
    // 模板字符，直接添加到patterns中，添加完成后，状态变为解析常规字符，%d特殊处理
    patterns.emplace_back(1, c);
    parsing_string = true;
    // parsing_pattern = false;

    // 后面是对%d的特殊处理，如果%d后面直接跟了一对大括号，那么把大括号里面的内容提取出来作为dateformat
    if (c != "d") {
      i++;
      continue;
    }
    i++;
    if (i < pattern_.size() && pattern_[i] != '{') {
      continue;
    }
    i++;
    while (i < pattern_.size() && pattern_[i] != '}') {
      dateformat.push_back(pattern_[i]);
      i++;
    }
    if (pattern_[i] != '}') {
      // %d后面的大括号没有闭合，直接报错
      error = true;
      break;
    }
    i++;

  }  // end while(i < m_pattern.size())

  if (error) {
    has_error_ = true;
    return;
  }

  // 模板解析结束之后剩余的常规字符也要算进去
  if (!tmp.empty()) {
    patterns.emplace_back(0, tmp);
    tmp.clear();
  }

  static std::unordered_map<std::string, std::function<FormatItem::s_ptr(const std::string &str)>> s_format_items = {
#define KV(str, C)                                                             \
  {                                                                            \
    #str, [](const std::string &fmt) { return FormatItem::s_ptr(new C(fmt)); } \
  }
      KV(m, MessageFormatItem),      // m:消息
      KV(p, LevelFormatItem),        // p:日志级别
      KV(c, LoggerNameFormatItem),   // c:日志器名称
      KV(d, DateTimeFormatItem),     // d:日期时间
      KV(r, ElapseFormatItem),       // r:累计毫秒数
      KV(f, FilenameFormatItem),     // f:文件名
      KV(l, LineFormatItem),         // l:行号
      KV(t, ThreadIdFormatItem),     // t:线程号
      KV(C, CoroutineIdFormatItem),  // C:协程号
      KV(N, ThreadNameFormatItem),   // N:线程名称
      KV(%, PercentFormatItem),      // %:百分号
      KV(T, TabFormatItem),          // T:制表符
      KV(n, NewLineFormatItem),      // n:换行符
#undef KV
  };

  for (auto &v : patterns) {
    if (v.first == 0) {
      items_.push_back(FormatItem::s_ptr(new StringFormatItem(v.second)));
    } else if (v.second == "d") {
      items_.push_back(FormatItem::s_ptr(new DateTimeFormatItem(dateformat)));
    } else {
      auto it = s_format_items.find(v.second);
      if (it == s_format_items.end()) {
        error = true;
        break;
      }
      items_.push_back(it->second(v.second));
    }
  }

  if (error) {
    has_error_ = true;
    return;
  }
}

auto LogFormatter::Format(const LogEvent::s_ptr &event) -> std::string {
  std::stringstream ss;
  for (auto &item : items_) {
    item->Format(ss, event);
  }
  return ss.str();
}

auto LogFormatter::Format(const LogEvent::s_ptr &event, std::ostream &os) -> std::ostream & {
  for (auto &item : items_) {
    item->Format(os, event);
  }
  return os;
}

LogAppender::LogAppender(LogFormatter::s_ptr formatter) : default_formatter_(std::move(formatter)) {}

void LogAppender::SetFormatter(LogFormatter::s_ptr formatter) {
  std::lock_guard<MutexType> lock(mutex_);
  formatter_ = std::move(formatter);
}

auto LogAppender::GetFormatter() const -> LogFormatter::s_ptr {
  std::lock_guard<MutexType> lock(mutex_);
  return formatter_ == nullptr ? default_formatter_ : formatter_;
}

StdoutLogAppender::StdoutLogAppender() : LogAppender(std::make_shared<LogFormatter>()) {}

void StdoutLogAppender::Log(LogEvent::s_ptr event) {
  if (formatter_ != nullptr) {
    formatter_->Format(event, std::cout);
  } else {
    default_formatter_->Format(event, std::cout);
  }
}

auto StdoutLogAppender::FlushConfigToYmal() -> std::string {
  std::lock_guard<MutexType> lock(mutex_);
  YAML::Node node;
  node["type"] = "Stdout";
  node["pattern"] = formatter_ == nullptr ? "" : formatter_->GetPattern();
  std::stringstream ss;
  ss << node;
  return ss.str();
}

FileLogAppender::FileLogAppender(std::string_view filename)
    : LogAppender(std::make_shared<LogFormatter>()), filename_(filename) {
  Reopen();
  if (open_error_) {
    std::cout << "open file " << filename_ << " failed" << std::endl;
  }
}

void FileLogAppender::Log(LogEvent::s_ptr event) {
  uint64_t now = event->GetTime();
  if (now >= (last_time_ + 3)) {
    Reopen();
    if (open_error_) {
      std::cout << "open file " << filename_ << " failed" << std::endl;
    }
    last_time_ = now;
  }

  if (open_error_) {
    return;
  }

  std::lock_guard<MutexType> lock(mutex_);
  if (formatter_ != nullptr) {
    if (!formatter_->Format(event, file_stream_)) {
      std::cout << "format log event failed" << std::endl;
    }
  } else {
    if (!default_formatter_->Format(event, file_stream_)) {
      std::cout << "format log event failed" << std::endl;
    }
  }
}

auto FileLogAppender::Reopen() -> bool {
  std::lock_guard<MutexType> lock(mutex_);
  if (file_stream_.is_open()) {
    file_stream_.close();
  }
  file_stream_.open(filename_, std::ios::app);
  open_error_ = !file_stream_.is_open();
  return !open_error_;
}

auto FileLogAppender::FlushConfigToYmal() -> std::string {
  std::lock_guard<MutexType> lock(mutex_);
  YAML::Node node;
  node["type"] = "File";
  node["file"] = filename_;
  node["pattern"] = formatter_ == nullptr ? "" : formatter_->GetPattern();
  std::stringstream ss;
  ss << node;
  return ss.str();
}

Logger::Logger(std::string_view name) : name_(name), level_(LogLevel::INFO), create_time_(GetElapsedTime()) {}

void Logger::AddAppender(LogAppender::s_ptr appender) {
  std::lock_guard<MutexType> lock(mutex_);
  appenders_.emplace_back(std::move(appender));
}

void Logger::RemoveAppender(const LogAppender::s_ptr &appender) {
  std::lock_guard<MutexType> lock(mutex_);
  for (auto it = appenders_.begin(); it != appenders_.end(); it++) {
    if (*it == appender) {
      appenders_.erase(it);
      break;
    }
  }
}

void Logger::ClearAppenders() {
  std::lock_guard<MutexType> lock(mutex_);
  appenders_.clear();
}

void Logger::SetLevel(LogLevel level) {
  std::lock_guard<MutexType> lock(mutex_);
  level_ = level;
}

auto Logger::GetLevel() const -> LogLevel {
  std::lock_guard<MutexType> lock(mutex_);
  return level_;
}

auto Logger::GetName() const -> std::string { return name_; }

auto Logger::GetCreateTime() const -> time_t { return create_time_; }

void Logger::Log(const LogEvent::s_ptr &event) {
  if (event->GetLevel() >= level_) {
    // 保证线程安全
    std::lock_guard<MutexType> lock(mutex_);
    for (auto &appender : appenders_) {
      appender->Log(event);
    }
  }
}

auto Logger::FlushConfigToYmal() -> std::string {
  std::lock_guard<MutexType> lock(mutex_);
  YAML::Node node;
  node["name"] = name_;
  node["level"] = ToString(level_);
  node["appenders"] = YAML::Load("[]");
  for (auto &i : appenders_) {
    node["appenders"].push_back(YAML::Load(i->FlushConfigToYmal()));
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

LogEventWrap::LogEventWrap(Logger::s_ptr logger, LogEvent::s_ptr event)
    : logger_(std::move(logger)), event_(std::move(event)) {}

LogEventWrap::~LogEventWrap() { logger_->Log(event_); }

LoggerManager::LoggerManager() {
  root_logger_ = std::make_shared<Logger>("root");
  root_logger_->AddAppender(std::make_shared<StdoutLogAppender>());
  loggers_[root_logger_->GetName()] = root_logger_;
}

auto LoggerManager::GetRoot() const -> Logger::s_ptr { return loggers_.at("root"); }

auto LoggerManager::GetLogger(const std::string &name) -> Logger::s_ptr {
  std::lock_guard<MutexType> lock(mutex_);

  auto it = loggers_.find(name);
  if (it != loggers_.end()) {
    return it->second;
  }

  return CreateEmptyLogger(name);
}

auto LoggerManager::CreateEmptyLogger(const std::string &name) -> Logger::s_ptr {
  auto logger = std::make_shared<Logger>(name);
  logger->AddAppender(std::make_shared<StdoutLogAppender>());
  loggers_.emplace(name, logger);
  return logger;
}

auto LoggerManager::FlushConfigToYmal() -> std::string {
  std::lock_guard<MutexType> lock(mutex_);
  YAML::Node node;
  for (auto &i : loggers_) {
    node.push_back(YAML::Load(i.second->FlushConfigToYmal()));
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

/**
 * @brief 从配置文件中加载日志配置
 *
 */
struct LogAppenderDefine {
  int type_ = 0;  // 1:File, 2:Stdout
  std::string pattern_;
  std::string filename_;

  auto operator==(const LogAppenderDefine &other) const -> bool {
    return type_ == other.type_ && pattern_ == other.pattern_ && filename_ == other.filename_;
  }
};

struct LoggerDefine {
  std::string name_;
  LogLevel level_ = LogLevel::UNKNOWN;
  std::vector<LogAppenderDefine> appenders_;

  auto operator==(const LoggerDefine &other) const -> bool {
    return name_ == other.name_ && level_ == other.level_ && appenders_ == other.appenders_;
  }

  auto operator<(const LoggerDefine &other) const -> bool { return name_ < other.name_; }

  auto IsValid() const -> bool { return !name_.empty(); }
};

template <>
class LexicalCast<std::string, LoggerDefine> {
 public:
  auto operator()(const std::string &v) {
    YAML::Node n = YAML::Load(v);
    LoggerDefine ld;
    if (!n["name"].IsDefined()) {
      std::cout << "log config error: name is null, " << n << std::endl;
      throw std::logic_error("log config error: name is null, " + v);
    }
    ld.name_ = n["name"].as<std::string>();
    ld.level_ = FromString(n["level"].IsDefined() ? n["level"].as<std::string>() : "");

    if (n["appenders"].IsDefined()) {
      for (size_t i = 0; i < n["appenders"].size(); i++) {
        auto a = n["appenders"][i];
        if (!a["type"].IsDefined()) {
          std::cout << "log config error: appender type is null, " << a << std::endl;
          continue;
        }
        auto type = a["type"].as<std::string>();
        LogAppenderDefine lad;
        if (type == "File") {
          lad.type_ = 1;
          if (!a["file"].IsDefined()) {
            std::cout << "log appender config error: file appender file is null, " << a << std::endl;
            continue;
          }
          lad.filename_ = a["file"].as<std::string>();
          if (a["pattern"].IsDefined()) {
            lad.pattern_ = a["pattern"].as<std::string>();
          }
        } else if (type == "Stdout") {
          lad.type_ = 2;
          if (a["pattern"].IsDefined()) {
            lad.pattern_ = a["pattern"].as<std::string>();
          }
        } else {
          std::cout << "log appender config error: appender type is invalid, " << a << std::endl;
          continue;
        }
        ld.appenders_.push_back(lad);
      }
    }
    return ld;
  }
};

template <>
class LexicalCast<LoggerDefine, std::string> {
 public:
  auto operator()(const LoggerDefine &i) -> std::string {
    YAML::Node n;
    n["name"] = i.name_;
    n["level"] = ToString(i.level_);
    for (auto &a : i.appenders_) {
      YAML::Node na;
      if (a.type_ == 1) {
        na["type"] = "File";
        na["file"] = a.filename_;
      } else if (a.type_ == 2) {
        na["type"] = "Stdout";
      }
      if (!a.pattern_.empty()) {
        na["pattern"] = a.pattern_;
      }
      n["appenders"].push_back(na);
    }
    std::stringstream ss;
    ss << n;
    return ss.str();
  }
};

ConfigItem<std::set<LoggerDefine>>::s_ptr g_log_define =
    ConfigMgr::GetInstance()->GetOrAddDefaultConfigItem("loggers", std::set<LoggerDefine>{}, "loggers");

struct LogIniter {
  LogIniter() {
    g_log_define->AddListener([](const std::set<LoggerDefine> &old_value, const std::set<LoggerDefine> &new_value) {
      LOG_INFO(ROOT_LOGGER) << "on log config changed";
      for (auto &i : new_value) {
        auto it = old_value.find(i);
        Logger::s_ptr logger;
        // 如果程序中定义了配置文件中未定义的Logger，那么创建一个空的Logger
        // 但是不能简单地检查old_value，因为在第一次加载配置文件时，old_value为空，但是却有可能程序中定义了Logger
        if (it == old_value.end() && LoggerMgr::GetInstance()->GetLogger(i.name_) == nullptr) {
          logger = LoggerMgr::GetInstance()->CreateEmptyLogger(i.name_);
        } else {
          if (!(i == *it)) {
            logger = LoggerMgr::GetInstance()->GetLogger(i.name_);
          } else {
            continue;
          }
        }
        logger->SetLevel(i.level_);
        logger->ClearAppenders();
        for (auto &a : i.appenders_) {
          LogAppender::s_ptr appender;
          if (a.type_ == 1) {
            appender = std::make_shared<FileLogAppender>(a.filename_);
          } else if (a.type_ == 2) {
            //  如果以守护进程方式运行，则不需要stdout
            if (!EnvMgr::GetInstance()->CheckArg("daemonize")) {
              appender = std::make_shared<StdoutLogAppender>();
            } else {
              continue;
            }
          }
          if (!a.pattern_.empty()) {
            appender->SetFormatter(std::make_shared<LogFormatter>(a.pattern_));
          } else {
            appender->SetFormatter(std::make_shared<LogFormatter>());
          }
          logger->AddAppender(appender);
        }
      }

      // 将之前的配置中，不存在于新配置中的Logger清空
      // 这里并不和652行的逻辑冲突，652行避免的情况是程序中定义了和配置文件中同名的Logger，但是配置文件中的配置项和程序中的不同
      for (auto &i : old_value) {
        auto it = new_value.find(i);
        if (it == new_value.end()) {
          Logger::s_ptr logger = LoggerMgr::GetInstance()->GetLogger(i.name_);
          logger->SetLevel(LogLevel::UNKNOWN);
          logger->ClearAppenders();
        }
      }
    });
  }
};

/**
 * @brief 在main函数之前初始化，构造函数中注册配置更改的回调函数，用于在更新配置是将Log相关的配置更新到ConfigManager中
 */
static LogIniter __LogIniter;  // NOLINT

}  // namespace wtsclwq