#pragma once

#include <array>
#include <cstddef>
#include <fcntl.h>
#if defined(__linux__)
#include <sys/epoll.h>
#elif defined(__FreeBSD__)
#include <sys/event.h>
#endif
#include <unistd.h>

namespace process {

  class FdMultiplexer {
  public:
    struct ReadyFds {
      std::array<int, 8> values{};
      std::size_t count = 0;
    };

    FdMultiplexer() {
#if defined(__linux__)
      m_fd = ::epoll_create1(EPOLL_CLOEXEC);
#elif defined(__FreeBSD__)
      m_fd = ::kqueue();
      if (m_fd >= 0) {
        const int flags = ::fcntl(m_fd, F_GETFD);
        if (flags < 0 || ::fcntl(m_fd, F_SETFD, flags | FD_CLOEXEC) != 0) {
          ::close(m_fd);
          m_fd = -1;
        }
      }
#endif
    }

    ~FdMultiplexer() {
      if (m_fd >= 0) {
        ::close(m_fd);
      }
    }

    FdMultiplexer(const FdMultiplexer&) = delete;
    FdMultiplexer& operator=(const FdMultiplexer&) = delete;

    [[nodiscard]] bool valid() const noexcept { return m_fd >= 0; }
    [[nodiscard]] int fd() const noexcept { return m_fd; }

    [[nodiscard]] bool add(int fd) const noexcept {
      if (!valid() || fd < 0) {
        return false;
      }
#if defined(__linux__)
      epoll_event event{};
      event.events = EPOLLIN;
      event.data.fd = fd;
      return ::epoll_ctl(m_fd, EPOLL_CTL_ADD, fd, &event) == 0;
#elif defined(__FreeBSD__)
      struct kevent event{};
      EV_SET(&event, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
      return ::kevent(m_fd, &event, 1, nullptr, 0, nullptr) == 0;
#else
      return false;
#endif
    }

    [[nodiscard]] ReadyFds ready() const noexcept {
      ReadyFds result;
      if (!valid()) {
        return result;
      }
#if defined(__linux__)
      epoll_event events[8]{};
      const int count = ::epoll_wait(m_fd, events, 8, 0);
      if (count > 0) {
        result.count = static_cast<std::size_t>(count);
        for (std::size_t index = 0; index < result.count; ++index) {
          result.values[index] = events[index].data.fd;
        }
      }
#elif defined(__FreeBSD__)
      struct kevent events[8]{};
      const struct timespec timeout{};
      const int count = ::kevent(m_fd, nullptr, 0, events, 8, &timeout);
      if (count > 0) {
        result.count = static_cast<std::size_t>(count);
        for (std::size_t index = 0; index < result.count; ++index) {
          result.values[index] = static_cast<int>(events[index].ident);
        }
      }
#endif
      return result;
    }

  private:
    int m_fd = -1;
  };

} // namespace process
