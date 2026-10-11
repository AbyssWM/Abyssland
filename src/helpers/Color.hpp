#pragma once

#include <cstdint>
#include <abyssgraphics/color/Color.hpp>
#include "../macros.hpp"

class CAbyssColor {
  public:
    CAbyssColor();
    CAbyssColor(float r, float g, float b, float a);
    CAbyssColor(const Abyssgraphics::CColor& col, float a);
    CAbyssColor(uint64_t);

    // AR32
    uint32_t                     getAsHex() const;
    Abyssgraphics::CColor::SSRGB  asRGB() const;
    Abyssgraphics::CColor::SOkLab asOkLab() const;
    Abyssgraphics::CColor::SHSL   asHSL() const;
    CAbyssColor                   stripA() const;
    CAbyssColor                   modifyA(float newa) const;

    //
    bool operator==(const CAbyssColor& c2) const {
        return c2.r == r && c2.g == g && c2.b == b && c2.a == a;
    }

    // stubs for the AnimationMgr
    CAbyssColor operator-(const CAbyssColor& c2) const {
        RASSERT(false, "CAbyssColor: - is a STUB");
        return {};
    }

    CAbyssColor operator+(const CAbyssColor& c2) const {
        RASSERT(false, "CAbyssColor: + is a STUB");
        return {};
    }

    CAbyssColor operator*(const float& c2) const {
        RASSERT(false, "CAbyssColor: * is a STUB");
        return {};
    }

    double r = 0, g = 0, b = 0, a = 0;

  private:
    Abyssgraphics::CColor::SOkLab m_okLab; // cache for the OkLab representation
};

//NOLINTNEXTLINE
namespace Colors {
    inline const CAbyssColor WHITE      = CAbyssColor(1.F, 1.F, 1.F, 1.F);
    inline const CAbyssColor GREEN      = CAbyssColor(0.F, 1.F, 0.F, 1.F);
    inline const CAbyssColor BLUE       = CAbyssColor(0.F, 0.F, 1.F, 1.F);
    inline const CAbyssColor RED        = CAbyssColor(1.F, 0.F, 0.F, 1.F);
    inline const CAbyssColor ORANGE     = CAbyssColor(1.F, 0.5F, 0.F, 1.F);
    inline const CAbyssColor YELLOW     = CAbyssColor(1.F, 1.F, 0.F, 1.F);
    inline const CAbyssColor MAGENTA    = CAbyssColor(1.F, 0.F, 1.F, 1.F);
    inline const CAbyssColor PURPLE     = CAbyssColor(0.5F, 0.F, 0.5F, 1.F);
    inline const CAbyssColor LIME       = CAbyssColor(0.5F, 1.F, 0.1F, 1.F);
    inline const CAbyssColor LIGHT_BLUE = CAbyssColor(0.1F, 1.F, 1.F, 1.F);
    inline const CAbyssColor BLACK      = CAbyssColor(0.F, 0.F, 0.F, 1.F);
};
