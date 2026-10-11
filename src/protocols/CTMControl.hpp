#pragma once

#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "abyssland-ctm-control-v1.hpp"
#include <unordered_map>
#include <map>
#include "../helpers/AnimatedVariable.hpp"

class CAbysslandCTMControlResource {
  public:
    CAbysslandCTMControlResource(UP<CAbysslandCtmControlManagerV1>&& resource_);
    ~CAbysslandCTMControlResource();

    bool good();
    void block();

  private:
    UP<CAbysslandCtmControlManagerV1>        m_resource;

    std::unordered_map<std::string, Mat3x3> m_ctms;
    bool                                    m_blocked = false;
};

class CAbysslandCTMControlProtocol : public IWaylandProtocol {
  public:
    CAbysslandCTMControlProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void destroyResource(CAbysslandCTMControlResource* resource);

    void setCTM(PHLMONITOR monitor, const Mat3x3& ctm);
    bool isCTMAnimationEnabled();

    //
    std::vector<UP<CAbysslandCTMControlResource>> m_managers;
    WP<CAbysslandCTMControlResource>              m_manager;

    //
    struct SCTMData {
        SCTMData();
        Mat3x3            ctmFrom = Mat3x3::identity(), ctmTo = Mat3x3::identity();
        PHLANIMVAR<float> progress;
    };
    std::map<PHLMONITORREF, UP<SCTMData>> m_ctmDatas;

    friend class CAbysslandCTMControlResource;
};

namespace PROTO {
    inline UP<CAbysslandCTMControlProtocol> ctm;
};
