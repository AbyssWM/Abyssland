#pragma once

#include <optional>
#include <sys/types.h>
#include <abyssutils/os/FileDescriptor.hpp>

namespace DRM {
    std::optional<dev_t>           devIDFromFD(int fd);
    bool                           sameGpu(int fd1, int fd2);
    int                            doIoctl(int fd, unsigned long request, void* arg);
    Abyssutils::OS::CFileDescriptor exportFence(int fd);
    Abyssutils::OS::CFileDescriptor mergeFence(const Abyssutils::OS::CFileDescriptor& fd1, const Abyssutils::OS::CFileDescriptor& fd2);
}
