/******************************************************************************
 * Copyright 2026 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include "cyber/io/session.h"

#include <poll.h>

#include <cerrno>

#include "cyber/common/log.h"

namespace apollo {
namespace cyber {
namespace io {

Session::Session() : Session(-1) {}

Session::Session(int fd) : fd_(fd) {}

int Session::Socket(int domain, int type, int protocol) {
  if (fd_ != -1) {
    AINFO << "session has hold a valid fd[" << fd_ << "]";
    return -1;
  }
  int sock_fd = socket(domain, type | SOCK_NONBLOCK, protocol);
  if (sock_fd != -1) {
    set_fd(sock_fd);
  }
  return sock_fd;
}

int Session::Listen(int backlog) {
  ACHECK(fd_ != -1);
  return listen(fd_, backlog);
}

int Session::Bind(const struct sockaddr *addr, socklen_t addrlen) {
  ACHECK(fd_ != -1);
  ACHECK(addr != nullptr);
  return bind(fd_, addr, addrlen);
}

Session::SessionPtr Session::Accept(struct sockaddr *addr, socklen_t *addrlen) {
  ACHECK(fd_ != -1);
  int sock_fd = accept4(fd_, addr, addrlen, SOCK_NONBLOCK);
  while (sock_fd == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
    if (!Wait(true, -1)) {
      return nullptr;
    }
    sock_fd = accept4(fd_, addr, addrlen, SOCK_NONBLOCK);
  }
  if (sock_fd == -1) {
    return nullptr;
  }
  return std::make_shared<Session>(sock_fd);
}

int Session::Connect(const struct sockaddr *addr, socklen_t addrlen) {
  ACHECK(fd_ != -1);
  int optval;
  socklen_t optlen = sizeof(optval);
  int res = connect(fd_, addr, addrlen);
  if (res == -1 && errno == EINPROGRESS) {
    Wait(false, -1);
    getsockopt(fd_, SOL_SOCKET, SO_ERROR, reinterpret_cast<void *>(&optval),
               &optlen);
    if (optval == 0) {
      res = 0;
    } else {
      errno = optval;
    }
  }
  return res;
}

int Session::Close() {
  ACHECK(fd_ != -1);
  int res = close(fd_);
  fd_ = -1;
  return res;
}

bool Session::Wait(bool read, int timeout_ms) {
  struct pollfd pfd;
  pfd.fd = fd_;
  pfd.events = read ? POLLIN : POLLOUT;
  pfd.revents = 0;
  while (true) {
    int res = poll(&pfd, 1, timeout_ms);
    if (res > 0) {
      return true;
    }
    if (res == 0) {
      errno = EAGAIN;
      return false;
    }
    if (errno != EINTR) {
      return false;
    }
  }
}

ssize_t Session::Recv(void *buf, size_t len, int flags, int timeout_ms) {
  ACHECK(buf != nullptr);
  ACHECK(fd_ != -1);
  ssize_t nbytes = recv(fd_, buf, len, flags);
  if (timeout_ms == 0) {
    return nbytes;
  }
  while (nbytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
    if (!Wait(true, timeout_ms)) {
      break;
    }
    nbytes = recv(fd_, buf, len, flags);
  }
  return nbytes;
}

ssize_t Session::RecvFrom(void *buf, size_t len, int flags,
                          struct sockaddr *src_addr, socklen_t *addrlen,
                          int timeout_ms) {
  ACHECK(buf != nullptr);
  ACHECK(fd_ != -1);
  ssize_t nbytes = recvfrom(fd_, buf, len, flags, src_addr, addrlen);
  if (timeout_ms == 0) {
    return nbytes;
  }
  while (nbytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
    if (!Wait(true, timeout_ms)) {
      break;
    }
    nbytes = recvfrom(fd_, buf, len, flags, src_addr, addrlen);
  }
  return nbytes;
}

ssize_t Session::Send(const void *buf, size_t len, int flags, int timeout_ms) {
  ACHECK(buf != nullptr);
  ACHECK(fd_ != -1);
  ssize_t nbytes = send(fd_, buf, len, flags);
  if (timeout_ms == 0) {
    return nbytes;
  }
  while (nbytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
    if (!Wait(false, timeout_ms)) {
      break;
    }
    nbytes = send(fd_, buf, len, flags);
  }
  return nbytes;
}

ssize_t Session::SendTo(const void *buf, size_t len, int flags,
                        const struct sockaddr *dest_addr, socklen_t addrlen,
                        int timeout_ms) {
  ACHECK(buf != nullptr);
  ACHECK(dest_addr != nullptr);
  ACHECK(fd_ != -1);
  ssize_t nbytes = sendto(fd_, buf, len, flags, dest_addr, addrlen);
  if (timeout_ms == 0) {
    return nbytes;
  }
  while (nbytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
    if (!Wait(false, timeout_ms)) {
      break;
    }
    nbytes = sendto(fd_, buf, len, flags, dest_addr, addrlen);
  }
  return nbytes;
}

ssize_t Session::Read(void *buf, size_t count, int timeout_ms) {
  ACHECK(buf != nullptr);
  ACHECK(fd_ != -1);
  ssize_t nbytes = read(fd_, buf, count);
  if (timeout_ms == 0) {
    return nbytes;
  }
  while (nbytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
    if (!Wait(true, timeout_ms)) {
      break;
    }
    nbytes = read(fd_, buf, count);
  }
  return nbytes;
}

ssize_t Session::Write(const void *buf, size_t count, int timeout_ms) {
  ACHECK(buf != nullptr);
  ACHECK(fd_ != -1);
  ssize_t nbytes = write(fd_, buf, count);
  if (timeout_ms == 0) {
    return nbytes;
  }
  while (nbytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
    if (!Wait(false, timeout_ms)) {
      break;
    }
    nbytes = write(fd_, buf, count);
  }
  return nbytes;
}

}  // namespace io
}  // namespace cyber
}  // namespace apollo
