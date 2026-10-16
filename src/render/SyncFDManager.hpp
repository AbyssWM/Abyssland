#pragma once

#include <abyssutils/os/FileDescriptor.hpp>

namespace Render {
    class ISyncFDManager {
      public:
        virtual ~ISyncFDManager() = default;

        Abyssutils::OS::CFileDescriptor&  fd();
        Abyssutils::OS::CFileDescriptor&& takeFd();
        virtual bool                      isValid();

      protected:
        ISyncFDManager() = default;

        Abyssutils::OS::CFileDescriptor m_fd;
        bool                            m_valid = false;
    };
}
