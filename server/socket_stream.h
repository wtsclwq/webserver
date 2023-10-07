#ifndef _WTSCLWQ_SOCKET_STREAM_
#define _WTSCLWQ_SOCKET_STREAM_

#include "server/socket.h"

namespace wtsclwq {
class SocketStream : public SocketWrap {
  ~SocketStream() override = default;
};
}  // namespace wtsclwq

#endif  // _WTSCLWQ_SOCKET_STREAM_