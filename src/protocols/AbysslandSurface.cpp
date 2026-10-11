#include "AbysslandSurface.hpp"
#include "../desktop/view/WLSurface.hpp"
#include "../render/Renderer.hpp"
#include "core/Compositor.hpp"
#include "abyssland-surface-v1.hpp"
#include <abyssutils/math/Region.hpp>
#include <wayland-server.h>

CAbysslandSurface::CAbysslandSurface(SP<CAbysslandSurfaceV1> resource, SP<CWLSurfaceResource> surface) : m_surface(surface) {
    setResource(std::move(resource));
}

bool CAbysslandSurface::good() const {
    return m_resource->resource();
}

void CAbysslandSurface::setResource(SP<CAbysslandSurfaceV1> resource) {
    m_resource = std::move(resource);

    if UNLIKELY (!m_resource->resource())
        return;

    m_resource->setDestroy([this](CAbysslandSurfaceV1* resource) { destroy(); });
    m_resource->setOnDestroy([this](CAbysslandSurfaceV1* resource) { destroy(); });

    m_resource->setSetOpacity([this](CAbysslandSurfaceV1* resource, uint32_t opacity) {
        if UNLIKELY (!m_surface) {
            m_resource->error(ABYSSLAND_SURFACE_V1_ERROR_NO_SURFACE, "set_opacity called for destroyed wl_surface");
            return;
        }

        auto fOpacity = wl_fixed_to_double(opacity);
        if UNLIKELY (fOpacity < 0.0 || fOpacity > 1.0) {
            m_resource->error(ABYSSLAND_SURFACE_V1_ERROR_OUT_OF_RANGE, "set_opacity called with an opacity value larger than 1.0 or smaller than 0.0.");
            return;
        }

        m_opacity = fOpacity;
    });

    m_resource->setSetVisibleRegion([this](CAbysslandSurfaceV1* resource, wl_resource* region) {
        if (!region) {
            if (!m_visibleRegion.empty())
                m_visibleRegionChanged = true;

            m_visibleRegion.clear();
            return;
        }

        m_visibleRegionChanged = true;
        m_visibleRegion        = CWLRegionResource::fromResource(region)->m_region;
    });

    m_listeners.surfaceCommitted = m_surface->m_events.commit.listen([this] {
        auto surface = Desktop::View::CWLSurface::fromResource(m_surface.lock());

        if (surface && (surface->m_overallOpacity != m_opacity || m_visibleRegionChanged)) {
            surface->m_overallOpacity = m_opacity;
            surface->m_visibleRegion  = m_visibleRegion;
            auto box                  = surface->getSurfaceBoxGlobal();

            if (box.has_value())
                g_pAbyssRenderer->damageBox(*box);

            if (!m_resource)
                PROTO::abysslandSurface->destroySurface(this);
        }
    });

    m_listeners.surfaceDestroyed = m_surface->m_events.destroy.listen([this] {
        if (!m_resource)
            PROTO::abysslandSurface->destroySurface(this);
    });
}

void CAbysslandSurface::destroy() {
    m_resource.reset();
    m_opacity = 1.F;

    if (!m_visibleRegion.empty())
        m_visibleRegionChanged = true;

    m_visibleRegion.clear();

    if (!m_surface)
        PROTO::abysslandSurface->destroySurface(this);
}

CAbysslandSurfaceProtocol::CAbysslandSurfaceProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CAbysslandSurfaceProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    auto manager = m_managers.emplace_back(makeUnique<CAbysslandSurfaceManagerV1>(client, ver, id)).get();
    manager->setOnDestroy([this](CAbysslandSurfaceManagerV1* manager) { destroyManager(manager); });

    manager->setDestroy([this](CAbysslandSurfaceManagerV1* manager) { destroyManager(manager); });
    manager->setGetAbysslandSurface(
        [this](CAbysslandSurfaceManagerV1* manager, uint32_t id, wl_resource* surface) { getSurface(manager, id, CWLSurfaceResource::fromResource(surface)); });
}

void CAbysslandSurfaceProtocol::destroyManager(CAbysslandSurfaceManagerV1* manager) {
    std::erase_if(m_managers, [&](const auto& p) { return p.get() == manager; });
}

void CAbysslandSurfaceProtocol::destroySurface(CAbysslandSurface* surface) {
    std::erase_if(m_surfaces, [&](const auto& entry) { return entry.second.get() == surface; });
}

void CAbysslandSurfaceProtocol::getSurface(CAbysslandSurfaceManagerV1* manager, uint32_t id, SP<CWLSurfaceResource> surface) {
    CAbysslandSurface* abysslandSurface = nullptr;
    auto              iter            = std::ranges::find_if(m_surfaces, [&](const auto& entry) { return entry.second->m_surface == surface; });

    if (iter != m_surfaces.end()) {
        if (iter->second->m_resource) {
            LOGM(Log::ERR, "AbysslandSurface already present for surface {:x}", (uintptr_t)surface.get());
            manager->error(ABYSSLAND_SURFACE_MANAGER_V1_ERROR_ALREADY_CONSTRUCTED, "AbysslandSurface already present");
            return;
        } else {
            iter->second->setResource(makeShared<CAbysslandSurfaceV1>(manager->client(), manager->version(), id));
            abysslandSurface = iter->second.get();
        }
    } else {
        abysslandSurface =
            m_surfaces.emplace(surface, makeUnique<CAbysslandSurface>(makeShared<CAbysslandSurfaceV1>(manager->client(), manager->version(), id), surface)).first->second.get();
    }

    if UNLIKELY (!abysslandSurface->good()) {
        manager->noMemory();
        m_surfaces.erase(surface);
    }
}
