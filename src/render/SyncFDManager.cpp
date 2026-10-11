#include "SyncFDManager.hpp"
#include <utility>

using namespace Render;

Abyssutils::OS::CFileDescriptor& ISyncFDManager::fd() {
    return m_fd;
}

Abyssutils::OS::CFileDescriptor&& ISyncFDManager::takeFd() {
    return std::move(m_fd);
}

bool ISyncFDManager::isValid() {
    return m_valid && m_fd.isValid();
}
