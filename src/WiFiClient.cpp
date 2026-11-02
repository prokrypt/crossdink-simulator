#if defined(CROSSPOINT_SIM_USE_SDK_HTTP)
#include "WiFiClient.h"
#include <cerrno>
#include <climits>
#include <cstdio>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
int WiFiClient::connect(const char *host, uint16_t port) {
  stop();
  char service[8];
  snprintf(service, sizeof(service), "%u", port);
  addrinfo hints{};
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;
  addrinfo *addresses = nullptr;
  if (!host || getaddrinfo(host, service, &hints, &addresses) != 0)
    return 0;
  for (auto *addr = addresses; addr; addr = addr->ai_next) {
    const int fd =
        socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (fd < 0)
      continue;
#ifdef SO_NOSIGPIPE
    int enabled = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
#endif
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    const int result = ::connect(fd, addr->ai_addr, addr->ai_addrlen);
    pollfd pending{fd, POLLOUT, 0};
    int error = 0;
    socklen_t length = sizeof(error);
    if (result == 0 ||
        (errno == EINPROGRESS &&
         poll(&pending, 1, static_cast<int>(connectTimeout_)) > 0 &&
         getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) == 0 &&
         error == 0)) {
      fd_ = fd;
      break;
    }
    ::close(fd);
  }
  freeaddrinfo(addresses);
  return fd_ >= 0;
}
size_t WiFiClient::write(const uint8_t *data, size_t size) {
  size_t sent = 0;
  while (fd_ >= 0 && sent < size) {
#ifdef MSG_NOSIGNAL
    constexpr int flags = MSG_NOSIGNAL;
#else
    constexpr int flags = 0;
#endif
    const auto n = send(fd_, data + sent, size - sent, flags);
    if (n > 0) {
      sent += n;
      continue;
    }
    if (n < 0 && errno == EINTR)
      continue;
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      pollfd pending{fd_, POLLOUT, 0};
      if (poll(&pending, 1, static_cast<int>(connectTimeout_)) > 0)
        continue;
    }
    stop();
  }
  return sent;
}
int WiFiClient::available() {
  int count = 0;
  if (fd_ >= 0)
    ioctl(fd_, FIONREAD, &count);
  return count;
}
int WiFiClient::read(uint8_t *data, size_t size) {
  if (fd_ < 0)
    return -1;
  const auto n = recv(fd_, data, size, 0);
  if (n == 0 ||
      (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR))
    stop();
  return static_cast<int>(n);
}
int WiFiClient::peek() {
  uint8_t b;
  return fd_ >= 0 && recv(fd_, &b, 1, MSG_PEEK) == 1 ? b : -1;
}
uint8_t WiFiClient::connected() {
  if (fd_ < 0)
    return false;
  uint8_t b;
  const auto n = recv(fd_, &b, 1, MSG_PEEK);
  if (n == 0 ||
      (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR))
    stop();
  return fd_ >= 0;
}
void WiFiClient::stop() {
  if (fd_ >= 0)
    ::close(fd_);
  fd_ = -1;
}

#endif
