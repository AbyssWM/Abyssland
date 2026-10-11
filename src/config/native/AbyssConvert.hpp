#pragma once

// Conversions from the public Abysslang value types (include/abyss/types.hpp) to what the native config cells take.
// Included by the generated OptionsApply.cpp and by the Abysslang implementation. Must not be included from the user config.

#include <cmath>
#include <string>
#include <string_view>

#include <abyss/types.hpp>
#include <abyss/Options.hpp>

#include "ConfigManager.hpp"

namespace abyss::detail {

    inline ::Config::Native::ConfigInput toInput(bool v) {
        return ::Config::Native::ConfigInput{std::in_place_type<bool>, v};
    }
    inline ::Config::Native::ConfigInput toInput(int64_t v) {
        return ::Config::Native::ConfigInput{std::in_place_type<int64_t>, v};
    }
    inline ::Config::Native::ConfigInput toInput(double v) {
        return ::Config::Native::ConfigInput{std::in_place_type<double>, v};
    }
    inline ::Config::Native::ConfigInput toInput(const std::string& v) {
        return ::Config::Native::ConfigInput{std::in_place_type<std::string>, v};
    }
    inline ::Config::Native::ConfigInput toInput(const Color& v) {
        return ::Config::Native::ConfigInput{std::in_place_type<CAbyssColor>, CAbyssColor(sc<uint64_t>(v.argb))};
    }
    inline ::Config::Native::ConfigInput toInput(const Vec2& v) {
        return ::Config::Native::ConfigInput{std::in_place_type<::Config::VEC2>, ::Config::VEC2{v.x, v.y}};
    }
    inline ::Config::Native::ConfigInput toInput(const Gap& v) {
        return ::Config::Native::ConfigInput{std::in_place_type<::Config::CCssGapData>, ::Config::CCssGapData(v.top, v.right, v.bottom, v.left)};
    }
    inline ::Config::Native::ConfigInput toInput(const Gradient& v) {
        std::vector<CAbyssColor> cols;
        for (const auto& c : v.colors)
            cols.emplace_back(sc<uint64_t>(c.argb));
        return ::Config::Native::ConfigInput{std::in_place_type<::Config::CGradientValueData>, ::Config::CGradientValueData(std::move(cols), sc<float>(v.angleDeg * M_PI / 180.0))};
    }
    inline ::Config::Native::ConfigInput toInput(const FontWeight& v) {
        if (const auto* i = std::get_if<int64_t>(&v.v))
            return toInput(*i);
        return toInput(std::get<std::string>(v.v));
    }
    inline ::Config::Native::ConfigInput toInput(const IntChoice& v) {
        if (const auto* i = std::get_if<int64_t>(&v.v))
            return toInput(*i);
        return toInput(std::get<std::string>(v.v));
    }

    // called by the generated applyOptions()
    template <typename T>
    void applyOpt(const char* name, const Opt<T>& o) {
        if (!o.isSet())
            return;
        if (const auto M = ::Config::Native::self(); M)
            M->setInput(name, toInput(o.get()));
    }
}
