
#include "utils.h"
#include <cxxabi.h>  // for abi::__cxa_demangle()
#include <dirent.h>
#include <execinfo.h>  // for backtrace()
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>
#include <algorithm>  // for std::transform()
#include <csignal>    // for kill()
#include <cstdarg>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string_view>

namespace wtsclwq {

auto GetCurrSysThreadId() -> pid_t { return syscall(SYS_gettid); }

// TODO(wtsclwq)
auto GetCurrCouroutineId() -> uint64_t { return 0; }

auto GetCurrSysThreadName() -> std::string {
  char name[16] = {0};
  pthread_getname_np(pthread_self(), name, sizeof(name));
  return name;
}

void SetCurrSysThreadName(std::string_view name) { pthread_setname_np(pthread_self(), name.substr(0, 15).data()); }

static auto Demangle(std::string_view str) -> std::string {
  size_t size = 0;
  int status = 0;
  std::string res;
  res.resize(256);
  if (1 == sscanf(str.data(), "%*[^(]%*[^_]%255[^)+]", res.data())) {
    char *ret = abi::__cxa_demangle(res.data(), nullptr, &size, &status);
    if (ret != nullptr) {
      std::string result(ret);
      free(ret);
      return result;
    }
  }
  if (1 == sscanf(str.data(), "%255s", res.data())) {
    return res;
  }
  return std::string(str);
}

void Backtrace(std::vector<std::string> *bt, int size, int skip) {
  void **array = static_cast<void **>(malloc(sizeof(void *) * size));
  size_t s = ::backtrace(array, size);
  char **strings = backtrace_symbols(array, s);

  if (strings == nullptr) {
    return;
  }

  for (size_t i = skip; i < s; ++i) {
    bt->push_back(Demangle(strings[i]));
  }

  free(strings);
  free(array);
}

auto BacktraceToString(int size, int skip, std::string_view prefix) -> std::string {
  std::vector<std::string> bt;
  Backtrace(&bt, size, skip);
  std::stringstream ss;
  for (const auto &i : bt) {
    ss << prefix << i << std::endl;
  }
  return ss.str();
}

auto GetCurrMs() -> uint64_t {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return tv.tv_sec * 1000UL + tv.tv_usec / 1000;
}

auto GetCurrUs() -> uint64_t {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return tv.tv_sec * 1000000UL + tv.tv_usec;
}

auto GetElapsedTime() -> int64_t {
  struct timespec ts {};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

auto ToUpper(std::string_view str) -> std::string {
  std::string result;
  result.resize(str.size());
  std::transform(str.begin(), str.end(), result.begin(), ::toupper);
  return result;
}

auto ToLower(std::string_view str) -> std::string {
  std::string result;
  result.resize(str.size());
  std::transform(str.begin(), str.end(), result.begin(), ::tolower);
  return result;
}

auto TimeToStr(time_t ts, std::string_view format) -> std::string {
  struct tm tm;
  localtime_r(&ts, &tm);
  char buf[64];
  strftime(buf, sizeof(buf), format.data(), &tm);
  return buf;
}

auto StrToTime(std::string_view str, std::string_view format) -> time_t {
  struct tm tm {};
  strptime(str.data(), format.data(), &tm);
  return mktime(&tm);
}

void FSUtil::ListAllFile(std::vector<std::string> *files, std::string_view path, std::string_view subfix) {
  if (access(path.data(), 0) != 0) {
    return;
  }
  DIR *dir = opendir(path.data());
  if (dir == nullptr) {
    return;
  }
  struct dirent *dp = nullptr;
  while ((dp = readdir(dir)) != nullptr) {
    if (dp->d_type == DT_DIR) {
      if ((strcmp(dp->d_name, ".") == 0) || (strcmp(dp->d_name, "..") == 0)) {
        continue;
      }
      ListAllFile(files, std::string(path) + "/" + dp->d_name, subfix);
    } else if (dp->d_type == DT_REG) {
      std::string filename(dp->d_name);
      if (subfix.empty()) {
        files->emplace_back(std::string(path) + "/" + filename);
      } else {
        if (filename.size() < subfix.size()) {
          continue;
        }
        if (filename.substr(filename.length() - subfix.length()) == subfix) {
          files->emplace_back(std::string(path) + "/" + filename);
        }
      }
    }
  }
  closedir(dir);
}

static auto __lstat(const char *file, struct stat *st = nullptr) -> int {  // NOLINT
  struct stat lst;
  int ret = lstat(file, &lst);
  if (st != nullptr) {
    *st = lst;
  }
  return ret;
}

static auto __mkdir(const char *dirname) -> int {  // NOLINT
  if (access(dirname, F_OK) == 0) {
    return 0;
  }
  return mkdir(dirname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

auto FSUtil::Mkdir(std::string_view dirname) -> bool {
  if (__lstat(dirname.data()) == 0) {
    return true;
  }
  char *path = strdup(dirname.data());
  char *ptr = strchr(path + 1, '/');
  do {
    *ptr = '\0';
    if (__mkdir(path) != 0) {
      free(path);
      return false;
    }
    *ptr = '/';
    ptr = strchr(ptr + 1, '/');
  } while (ptr != nullptr);
  if (__mkdir(path) != 0) {
    free(path);
    return false;
  }
  free(path);
  return true;
}

auto FSUtil::IsRunningPidfile(std::string_view pidfile) -> bool {
  if (access(pidfile.data(), F_OK) != 0) {
    return false;
  }
  std::ifstream ifs(pidfile.data());
  std::string line;
  if (!ifs || !std::getline(ifs, line)) {
    return false;
  }
  if (line.empty()) {
    return false;
  }
  pid_t pid = std::stoi(line);
  if (pid <= 1) {
    return false;
  }
  if (kill(pid, 0) != 0) {
    return false;
  }
  return true;
}

auto FSUtil::Unlink(std::string_view filename, bool exist) -> bool {
  if (!exist && access(filename.data(), F_OK) != 0) {
    return true;
  }
  return unlink(filename.data()) == 0;
}

auto FSUtil::Rm(std::string_view path) -> bool {
  struct stat st;
  if (lstat(path.data(), &st) != 0) {
    return true;
  }
  if ((st.st_mode & S_IFDIR) == 0U) {
    return Unlink(path);
  }

  DIR *dir = opendir(path.data());
  if (dir == nullptr) {
    return false;
  }

  bool ret = true;
  struct dirent *dp = nullptr;
  while ((dp = readdir(dir)) != nullptr) {
    if ((strcmp(dp->d_name, ".") == 0) || (strcmp(dp->d_name, "..") == 0)) {
      continue;
    }
    std::string dirname = std::string(path) + "/" + dp->d_name;
    ret = Rm(dirname);
  }
  closedir(dir);
  if (rmdir(path.data()) != 0) {
    ret = false;
  }
  return ret;
}

auto FSUtil::Mv(std::string_view from, std::string_view to) -> bool {
  if (!Rm(to)) {
    return false;
  }
  return rename(from.data(), to.data()) == 0;
}

auto FSUtil::Realpath(std::string_view path, std::string *rpath) -> bool {
  if (rpath == nullptr) {
    return false;
  }
  char *ptr = ::realpath(path.data(), nullptr);
  if (ptr == nullptr) {
    return false;
  }
  std::string result(ptr);
  free(ptr);
  *rpath = result;
  return true;
}

auto FSUtil::Symlink(std::string_view from, std::string_view to) -> bool {
  if (access(to.data(), F_OK) == 0) {
    return false;
  }
  return symlink(from.data(), to.data()) == 0;
}

auto FSUtil::Dirname(std::string_view filename) -> std::string {
  if (filename.empty()) {
    return ".";
  }
  auto pos = filename.rfind('/');
  if (pos == 0) {
    return "/";
  }
  if (pos == std::string_view::npos) {
    return ".";
  }
  return std::string(filename.substr(0, pos));
}

auto FSUtil::OpenForRead(std::ifstream &ifs, const std::string &filename, std::ios_base::openmode mode) -> bool {
  ifs.open(filename, mode);
  return ifs.is_open();
}

auto FSUtil::OpenForWrite(std::ofstream &ofs, const std::string &filename, std::ios_base::openmode mode) -> bool {
  ofs.open(filename, mode);
  if (!ofs.is_open()) {
    FSUtil::Mkdir(FSUtil::Dirname(filename));
    ofs.open(filename, mode);
  }
  return ofs.is_open();
}

auto StringUtil::Format(const char *fmt, ...) -> std::string {
  va_list ap;
  va_start(ap, fmt);
  auto result = Formatv(fmt, ap);
  va_end(ap);
  return result;
}

auto StringUtil::Formatv(const char *fmt, va_list ap) -> std::string {
  char *buf = nullptr;
  auto len = vasprintf(&buf, fmt, ap);
  if (len == -1) {
    return "";
  }
  std::string result(buf, len);
  free(buf);
  return result;
}

static const char URI_CHARS[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const char XDIGIT_CHARS[256] = {
    0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,
    0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

#define CHAR_IS_UNRESERVED(c) (URI_CHARS[(unsigned char)(c)])

//-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz~
auto StringUtil::UrlEncode(std::string_view str, bool space_as_plus) -> std::string {
  static const char *hexdigits = "0123456789ABCDEF";
  std::string *ss = nullptr;
  const char *end = str.data() + str.length();
  for (const char *c = str.data(); c < end; ++c) {
    if (!CHAR_IS_UNRESERVED(*c)) {
      if (ss == nullptr) {
        ss = new std::string;
        ss->reserve(str.size() * 1.2);
        ss->append(str.data(), c - str.data());
      }
      if (*c == ' ' && space_as_plus) {
        ss->append(1, '+');
      } else {
        ss->append(1, '%');
        ss->append(1, hexdigits[static_cast<uint8_t>(*c) >> 4]);
        ss->append(1, hexdigits[*c & 0xf]);
      }
    } else if (ss != nullptr) {
      ss->append(1, *c);
    }
  }
  if (ss == nullptr) {
    return std::string(str);
  }
  std::string rt = *ss;
  delete ss;
  return rt;
}

auto StringUtil::UrlDecode(std::string_view str, bool space_as_plus) -> std::string {
  std::string *ss = nullptr;
  const char *end = str.data() + str.length();
  for (const char *c = str.data(); c < end; ++c) {
    if (*c == '+' && space_as_plus) {
      if (ss == nullptr) {
        ss = new std::string;
        ss->append(str.data(), c - str.data());
      }
      ss->append(1, ' ');
    } else if (*c == '%' && (c + 2) < end && (isxdigit(*(c + 1)) != 0) && (isxdigit(*(c + 2)) != 0)) {
      if (ss == nullptr) {
        ss = new std::string;
        ss->append(str.data(), c - str.data());
      }
      ss->append(1, static_cast<char>(XDIGIT_CHARS[static_cast<int>(*(c + 1))] << 4 |
                                      XDIGIT_CHARS[static_cast<int>(*(c + 2))]));
      c += 2;
    } else if (ss != nullptr) {
      ss->append(1, *c);
    }
  }
  if (ss == nullptr) {
    return std::string(str);
  }
  std::string rt = *ss;
  delete ss;
  return rt;
}

auto StringUtil::Trim(std::string_view str, std::string_view delimit) -> std::string {
  auto begin = str.find_first_not_of(delimit);
  if (begin == std::string::npos) {
    return "";
  }
  auto end = str.find_last_not_of(delimit);
  return std::string(str.substr(begin, end - begin + 1));
}

auto StringUtil::TrimLeft(std::string_view str, std::string_view delimit) -> std::string {
  auto begin = str.find_first_not_of(delimit);
  if (begin == std::string::npos) {
    return "";
  }
  return std::string(str.substr(begin));
}

auto StringUtil::TrimRight(std::string_view str, std::string_view delimit) -> std::string {
  auto end = str.find_last_not_of(delimit);
  if (end == std::string::npos) {
    return "";
  }
  return std::string(str.substr(0, end));
}

auto StringUtil::WStringToString(const std::wstring &ws) -> std::string {
  std::string str_locale = setlocale(LC_ALL, "");
  const wchar_t *wch_src = ws.c_str();
  size_t n_dest_size = wcstombs(nullptr, wch_src, 0) + 1;
  char *ch_dest = new char[n_dest_size];
  memset(ch_dest, 0, n_dest_size);
  wcstombs(ch_dest, wch_src, n_dest_size);
  std::string str_result = ch_dest;
  delete[] ch_dest;
  setlocale(LC_ALL, str_locale.c_str());
  return str_result;
}

auto StringUtil::StringToWString(std::string_view s) -> std::wstring {
  std::string str_locale = setlocale(LC_ALL, "");
  const char *ch_src = s.data();
  size_t n_dest_size = mbstowcs(nullptr, ch_src, 0) + 1;
  auto *wch_dest = new wchar_t[n_dest_size];
  wmemset(wch_dest, 0, n_dest_size);
  mbstowcs(wch_dest, ch_src, n_dest_size);
  std::wstring wstr_result = wch_dest;
  delete[] wch_dest;
  setlocale(LC_ALL, str_locale.c_str());
  return wstr_result;
}

}  // namespace wtsclwq