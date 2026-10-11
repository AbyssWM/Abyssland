#include "Color.hpp"

#define ALPHA(c) ((double)(((c) >> 24) & 0xff) / 255.0)
#define RED(c)   ((double)(((c) >> 16) & 0xff) / 255.0)
#define GREEN(c) ((double)(((c) >> 8) & 0xff) / 255.0)
#define BLUE(c)  ((double)(((c)) & 0xff) / 255.0)

CAbyssColor::CAbyssColor() = default;

CAbyssColor::CAbyssColor(float r_, float g_, float b_, float a_) : r(r_), g(g_), b(b_), a(a_) {
    m_okLab = Abyssgraphics::CColor(Abyssgraphics::CColor::SSRGB{r, g, b}).asOkLab();
}

CAbyssColor::CAbyssColor(uint64_t hex) : r(RED(hex)), g(GREEN(hex)), b(BLUE(hex)), a(ALPHA(hex)) {
    m_okLab = Abyssgraphics::CColor(Abyssgraphics::CColor::SSRGB{r, g, b}).asOkLab();
}

CAbyssColor::CAbyssColor(const Abyssgraphics::CColor& color, float a_) : a(a_) {
    const auto SRGB = color.asRgb();
    r               = SRGB.r;
    g               = SRGB.g;
    b               = SRGB.b;

    m_okLab = color.asOkLab();
}

uint32_t CAbyssColor::getAsHex() const {
    return sc<uint32_t>(a * 255.f) * 0x1000000 + sc<uint32_t>(r * 255.f) * 0x10000 + sc<uint32_t>(g * 255.f) * 0x100 + sc<uint32_t>(b * 255.f) * 0x1;
}

Abyssgraphics::CColor::SSRGB CAbyssColor::asRGB() const {
    return {r, g, b};
}

Abyssgraphics::CColor::SOkLab CAbyssColor::asOkLab() const {
    return m_okLab;
}

Abyssgraphics::CColor::SHSL CAbyssColor::asHSL() const {
    return Abyssgraphics::CColor(m_okLab).asHSL();
}

CAbyssColor CAbyssColor::stripA() const {
    return {r, g, b, 1.F};
}

CAbyssColor CAbyssColor::modifyA(float newa) const {
    return {r, g, b, newa};
}
