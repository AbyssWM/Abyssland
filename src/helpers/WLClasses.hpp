#pragma once

#include "../defines.hpp"
#include "../desktop/view/Subsurface.hpp"
#include "../desktop/view/Popup.hpp"
#include "../desktop/view/WLSurface.hpp"
#include "../macros.hpp"
#include "../desktop/DesktopTypes.hpp"
#include "memory/Memory.hpp"
#include "signal/Signal.hpp"

class IPointer;
class IKeyboard;
class CWLSurfaceResource;

ABYSSAL_FORWARD(ISwitch);

struct SSwitchDevice {
    WP<Abyssal::ISwitch> pDevice;

    struct {
        CAbyssSignalListener destroy;
        CAbyssSignalListener fire;
    } listeners;

    bool operator==(const SSwitchDevice& other) const {
        return pDevice == other.pDevice;
    }
};
