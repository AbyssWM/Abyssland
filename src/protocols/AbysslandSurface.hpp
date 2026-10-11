#pragma once

#include <abyssutils/math/Region.hpp>
#include <vector>
#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "abyssland-surface-v1.hpp"
#include "../helpers/signal/Signal.hpp"

class CWLSurfaceResource;
class CAbysslandSurfaceProtocol;

class CAbysslandSurface {
  public:
    CAbysslandSurface(SP<CAbysslandSurfaceV1> resource, SP<CWLSurfaceResource> surface);

    bool good() const;
    void setResource(SP<CAbysslandSurfaceV1> resource);

  private:
    SP<CAbysslandSurfaceV1> m_resource;
    WP<CWLSurfaceResource> m_surface;
    float                  m_opacity              = 1.0;
    bool                   m_visibleRegionChanged = false;
    CRegion                m_visibleRegion;

    void                   destroy();

    struct {
        CAbyssSignalListener surfaceCommitted;
        CAbyssSignalListener surfaceDestroyed;
    } m_listeners;

    friend class CAbysslandSurfaceProtocol;
};

class CAbysslandSurfaceProtocol : public IWaylandProtocol {
  public:
    CAbysslandSurfaceProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void                                                             destroyManager(CAbysslandSurfaceManagerV1* res);
    void                                                             destroySurface(CAbysslandSurface* surface);
    void                                                             getSurface(CAbysslandSurfaceManagerV1* manager, uint32_t id, SP<CWLSurfaceResource> surface);

    std::vector<UP<CAbysslandSurfaceManagerV1>>                       m_managers;
    std::unordered_map<WP<CWLSurfaceResource>, UP<CAbysslandSurface>> m_surfaces;

    friend class CAbysslandSurface;
};

namespace PROTO {
    inline UP<CAbysslandSurfaceProtocol> abysslandSurface;
}
