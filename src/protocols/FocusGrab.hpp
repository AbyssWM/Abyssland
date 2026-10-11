#pragma once

#include "WaylandProtocol.hpp"
#include "abyssland-focus-grab-v1.hpp"
#include "../macros.hpp"
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "../helpers/signal/Signal.hpp"

class CFocusGrab;
class CSeatGrab;
class CWLSurfaceResource;

class CFocusGrabSurfaceState {
  public:
    CFocusGrabSurfaceState(CFocusGrab* grab, SP<CWLSurfaceResource> surface);
    ~CFocusGrabSurfaceState() = default;

    enum State {
        PendingAddition,
        PendingRemoval,
        Committed,
    } m_state = PendingAddition;

  private:
    struct {
        CAbyssSignalListener destroy;
    } m_listeners;
};

class CFocusGrab {
  public:
    CFocusGrab(SP<CAbysslandFocusGrabV1> resource_);
    ~CFocusGrab();

    bool good();
    bool isSurfaceCommitted(SP<CWLSurfaceResource> surface);

    void start();
    void finish(bool sendCleared);

  private:
    void                                                                   addSurface(SP<CWLSurfaceResource> surface);
    void                                                                   removeSurface(SP<CWLSurfaceResource> surface);
    void                                                                   eraseSurface(SP<CWLSurfaceResource> surface);
    void                                                                   refocusKeyboard();
    void                                                                   commit(bool removeOnly = false);

    SP<CAbysslandFocusGrabV1>                                               m_resource;
    std::unordered_map<WP<CWLSurfaceResource>, UP<CFocusGrabSurfaceState>> m_surfaces;
    SP<CSeatGrab>                                                          m_grab;

    bool                                                                   m_grabActive = false;

    friend class CFocusGrabSurfaceState;
};

class CFocusGrabProtocol : public IWaylandProtocol {
  public:
    CFocusGrabProtocol(const wl_interface* iface, const int& var, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void                                         onManagerResourceDestroy(wl_resource* res);
    void                                         destroyGrab(CFocusGrab* grab);
    void                                         onCreateGrab(CAbysslandFocusGrabManagerV1* pMgr, uint32_t id);

    std::vector<UP<CAbysslandFocusGrabManagerV1>> m_managers;
    std::vector<UP<CFocusGrab>>                  m_grabs;

    friend class CFocusGrab;
};

namespace PROTO {
    inline UP<CFocusGrabProtocol> focusGrab;
}
