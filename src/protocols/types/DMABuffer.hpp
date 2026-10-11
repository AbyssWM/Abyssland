#pragma once

#include "Buffer.hpp"
#include <abyssutils/os/FileDescriptor.hpp>

class CDMABuffer : public IHLBuffer {
  public:
    CDMABuffer(uint32_t id, wl_client* client, Abyssal::SDMABUFAttrs const& attrs_);
    virtual ~CDMABuffer();

    virtual Abyssal::eBufferCapability          caps();
    virtual Abyssal::eBufferType                type();
    virtual bool                                   isSynchronous();
    virtual void                                   update(const CRegion& damage);
    virtual Abyssal::SDMABUFAttrs               dmabuf();
    virtual std::tuple<uint8_t*, uint32_t, size_t> beginDataPtr(uint32_t flags);
    virtual void                                   endDataPtr();
    bool                                           good();
    void                                           closeFDs();
    std::vector<Abyssutils::OS::CFileDescriptor>    exportSyncFiles();
    bool                                           m_success = false;

  private:
    Abyssal::SDMABUFAttrs m_attrs;

    struct {
        CAbyssSignalListener resourceDestroy;
    } m_listeners;
};
