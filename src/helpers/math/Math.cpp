#include "Math.hpp"
#include "../memory/Memory.hpp"
#include "../../macros.hpp"

#include <unordered_map>
#include <array>

using namespace Math;

// FIXME: expose in hu
static std::unordered_map<eTransform, Mat3x3> transforms = {
    {ABYSSUTILS_TRANSFORM_NORMAL, std::array<float, 9>{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}},
    {ABYSSUTILS_TRANSFORM_90, std::array<float, 9>{0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f}},
    {ABYSSUTILS_TRANSFORM_180, std::array<float, 9>{-1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f}},
    {ABYSSUTILS_TRANSFORM_270, std::array<float, 9>{0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f}},
    {ABYSSUTILS_TRANSFORM_FLIPPED, std::array<float, 9>{-1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}},
    {ABYSSUTILS_TRANSFORM_FLIPPED_90, std::array<float, 9>{0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f}},
    {ABYSSUTILS_TRANSFORM_FLIPPED_180, std::array<float, 9>{1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f}},
    {ABYSSUTILS_TRANSFORM_FLIPPED_270, std::array<float, 9>{0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f}},
};

eTransform Math::wlTransformToAbyssutils(wl_output_transform t) {
    switch (t) {
        case WL_OUTPUT_TRANSFORM_NORMAL: return eTransform::ABYSSUTILS_TRANSFORM_NORMAL;
        case WL_OUTPUT_TRANSFORM_180: return eTransform::ABYSSUTILS_TRANSFORM_180;
        case WL_OUTPUT_TRANSFORM_90: return eTransform::ABYSSUTILS_TRANSFORM_90;
        case WL_OUTPUT_TRANSFORM_270: return eTransform::ABYSSUTILS_TRANSFORM_270;
        case WL_OUTPUT_TRANSFORM_FLIPPED: return eTransform::ABYSSUTILS_TRANSFORM_FLIPPED;
        case WL_OUTPUT_TRANSFORM_FLIPPED_180: return eTransform::ABYSSUTILS_TRANSFORM_FLIPPED_180;
        case WL_OUTPUT_TRANSFORM_FLIPPED_270: return eTransform::ABYSSUTILS_TRANSFORM_FLIPPED_270;
        case WL_OUTPUT_TRANSFORM_FLIPPED_90: return eTransform::ABYSSUTILS_TRANSFORM_FLIPPED_90;
        default: break;
    }
    return eTransform::ABYSSUTILS_TRANSFORM_NORMAL;
}

wl_output_transform Math::invertTransform(wl_output_transform tr) {
    if ((tr & WL_OUTPUT_TRANSFORM_90) && !(tr & WL_OUTPUT_TRANSFORM_FLIPPED))
        tr = sc<wl_output_transform>(tr ^ sc<int>(WL_OUTPUT_TRANSFORM_180));

    return tr;
}

static bool matEq(const Mat3x3& a, const Mat3x3& b) {
    for (size_t i = 0; i < 9; ++i) {
        const float Δ = std::fabs(a.getMatrix()[i] - b.getMatrix()[i]);
        if (Δ > 1e-6) // eps
            return false;
    }
    return true;
}

static eTransform composeInternal(eTransform a, eTransform b) {
    const auto& A      = transforms.at(a);
    const auto& B      = transforms.at(b);
    const auto  RESULT = Mat3x3{A}.multiply(B);

    for (const auto& [t, M] : transforms) {
        if (matEq(M, RESULT))
            return t;
    }

    return eTransform::ABYSSUTILS_TRANSFORM_NORMAL;
}

eTransform Math::composeTransform(eTransform a, eTransform b) {
    static std::array<std::array<eTransform, 8>, 8> lookup;
    static bool                                     once = true;

    if (once) {
        once = false;

        // bake the composition table
        static_assert(ABYSSUTILS_TRANSFORM_FLIPPED_270 == 7);
        for (size_t i = 0; i <= ABYSSUTILS_TRANSFORM_FLIPPED_270 /* 7 */; ++i) {
            for (size_t j = 0; j <= ABYSSUTILS_TRANSFORM_FLIPPED_270 /* 7 */; ++j) {
                lookup[i][j] = composeInternal(sc<eTransform>(i), sc<eTransform>(j));
            }
        }
    }

    RASSERT(a >= ABYSSUTILS_TRANSFORM_NORMAL && a <= ABYSSUTILS_TRANSFORM_FLIPPED_270, "Invalid transform a in composeTransform");
    RASSERT(b >= ABYSSUTILS_TRANSFORM_NORMAL && b <= ABYSSUTILS_TRANSFORM_FLIPPED_270, "Invalid transform b in composeTransform");

    return lookup[a][b];
}
