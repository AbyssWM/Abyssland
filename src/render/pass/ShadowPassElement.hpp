#pragma once
#include "PassElement.hpp"

class CAbyssDropShadowDecoration;

class CShadowPassElement : public IPassElement {
  public:
    struct SShadowData {
        CAbyssDropShadowDecoration* deco = nullptr;
        float                       a    = 1.F;
    };

    CShadowPassElement(const SShadowData& data_);
    virtual ~CShadowPassElement() = default;

    virtual bool        needsLiveBlur();
    virtual bool        needsPrecomputeBlur();

    virtual const char* passName() {
        return "CShadowPassElement";
    }

    virtual ePassElementType type() {
        return EK_SHADOW;
    };

    SShadowData m_data;
};
