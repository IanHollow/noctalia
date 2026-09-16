#pragma once

#include <cstddef>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <sys/mman.h>
#include <unistd.h>

namespace core {

  [[nodiscard]] inline int createAnonymousFile(std::string_view name, std::size_t size, bool allowSealing = false) {
#if defined(__linux__)
    const unsigned int flags = MFD_CLOEXEC | (allowSealing ? MFD_ALLOW_SEALING : 0U);
    const int fd = ::memfd_create(std::string{name}.c_str(), flags);
#elif defined(__FreeBSD__)
    (void)name;
    (void)allowSealing;
    const int fd = ::shm_open(SHM_ANON, O_RDWR | O_CLOEXEC, 0600);
#else
    (void)name;
    (void)allowSealing;
    const int fd = -1;
#endif
    if (fd < 0) {
      return -1;
    }
    if (::ftruncate(fd, static_cast<off_t>(size)) != 0) {
      ::close(fd);
      return -1;
    }
    return fd;
  }

} // namespace core
