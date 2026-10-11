#pragma once

#include "../../helpers/math/Math.hpp"

#include <string>

namespace Config {
    class ICustomConfigValueData;

    // 2D float vector used by vec2 options
    struct SVec2 {
        float x = 0, y = 0;
    };

    typedef bool                    BOOL;
    typedef int64_t                 INTEGER;
    typedef float                   FLOAT;
    typedef SVec2                   VEC2;
    typedef std::string             STRING;
    typedef ICustomConfigValueData* COMPLEX;
};