#include "fd_manager.h"
#include <asm-generic/socket.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include "hook.h"

namespace wtsclwq {

FileInfoWrapper::FileInfoWrapper(int fd) : sys_fd_(fd) { Init(); }

FileInfoWrapper::~FileInfoWrapper() {
  if (sys_fd_ != -1) {
    close_f(sys_fd_);
  }
}

auto FileInfoWrapper::Init() -> bool {
  if (is_inited_) {
    return true;
  }

  struct stat fd_stat;
  if (fstat(sys_fd_, &fd_stat) == -1) {
    is_inited_ = false;
    is_socket_ = false;
  } else {
    is_inited_ = true;
    is_socket_ = S_ISSOCK(fd_stat.st_mode);
  }

  // 如果是sockfd，那么默认设置为非阻塞
  if (is_socket_) {
    int flags = fcntl_f(sys_fd_, F_GETFL, 0);
    if ((flags & O_NONBLOCK) == 0) {
      fcntl_f(sys_fd_, F_SETFL, flags | O_NONBLOCK);  // 设置为非阻塞
    }
    is_sys_non_block_ = true;
  } else {
    // 如果不是socket，那么默认设置为阻塞
    is_sys_non_block_ = false;
  }

  // 虽然我们默认将sockfd在sys层面设置为了非阻塞，但是用户可能在使用时需要使用阻塞，
  // 所以我们需要记录用户是否手动设置了非阻塞，只有sys和user同时为非阻塞时，才认为是非阻塞
  is_user_non_block_ = false;
  is_closed_ = false;
  return is_inited_;
}

void FileInfoWrapper::SetTimeout(int type, uint64_t timeout_ms) {
  if (type == SO_RCVTIMEO) {
    read_timeout_ms_ = timeout_ms;
  } else if (type == SO_SNDTIMEO) {
    write_timeout_ms_ = timeout_ms;
  }
}

auto FileInfoWrapper::GetTimeout(int type) -> uint64_t {
  if (type == SO_RCVTIMEO) {
    return read_timeout_ms_;
  }
  return write_timeout_ms_;
}

auto FileInfoWrapper::IsInited() -> bool { return is_inited_; }

auto FileInfoWrapper::IsSocket() -> bool { return is_socket_; }

auto FileInfoWrapper::IsClosed() -> bool { return is_closed_; }

void FileInfoWrapper::SetUserLevelNonBlock(bool v) { is_user_non_block_ = v; }

auto FileInfoWrapper::IsUserLevelNonBlock() -> bool { return is_user_non_block_; }

void FileInfoWrapper::SetSysLevelNonBlock(bool v) { is_sys_non_block_ = v; }

auto FileInfoWrapper::IsSysLevelNonBlock() -> bool { return is_sys_non_block_; }

FileInfoWrapperManager::FileInfoWrapperManager() { fd_infos_.resize(64); }

FileInfoWrapperManager::~FileInfoWrapperManager() = default;

auto FileInfoWrapperManager::Get(int fd, bool auto_create) -> FileInfoWrapper::s_ptr {
  if (fd < 0) {
    return nullptr;
  }
  {
    std::shared_lock<MutexType> lock(mutex_);

    if (static_cast<size_t>(fd) >= fd_infos_.size()) {
      if (!auto_create) {
        return nullptr;
      }
    } else {
      if (fd_infos_[fd] != nullptr || !auto_create) {
        return fd_infos_[fd];
      }
    }
  }
  std::lock_guard<MutexType> lock(mutex_);
  auto res = std::make_shared<FileInfoWrapper>(fd);
  if (static_cast<size_t>(fd) >= fd_infos_.size()) {
    fd_infos_.resize(fd * 2);
  }
  fd_infos_[fd] = res;
  return res;
}

void FileInfoWrapperManager::Remove(int fd) {
  std::lock_guard<MutexType> lock_guard(mutex_);
  if (static_cast<size_t>(fd) >= fd_infos_.size()) {
    return;
  }
  fd_infos_[fd].reset();
}

}  // namespace wtsclwq