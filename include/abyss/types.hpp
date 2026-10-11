// Abysslang value types: colors, gradients, gaps, modifiers and the Opt<T> wrapper used by the generated option structs.
// Intentionally free of compositor internals so that it can be included anywhere.
#pragma once

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace abyss {

    // ---- colors ----

    // 0xAARRGGBB
    struct Color {
        uint32_t argb = 0xFF000000;
    };

    constexpr Color argb(uint32_t v) {
        return {v};
    }
    // rgb(0xRRGGBB): opaque
    constexpr Color rgb(uint32_t v) {
        return {0xFF000000u | (v & 0xFFFFFFu)};
    }
    constexpr Color rgb(uint8_t r, uint8_t g, uint8_t b) {
        return {0xFF000000u | (uint32_t(r) << 16) | (uint32_t(g) << 8) | b};
    }
    // rgba(0xRRGGBB, 0xAA) with an alpha of 0..255
    constexpr Color rgba(uint32_t v, int alpha) {
        return {((uint32_t(alpha) & 0xFFu) << 24) | (v & 0xFFFFFFu)};
    }
    // rgba(0xRRGGBB, 0.5) with an alpha of 0.0..1.0
    constexpr Color rgba(uint32_t v, double alpha) {
        return {(uint32_t(alpha * 255.0 + 0.5) << 24) | (v & 0xFFFFFFu)};
    }

    // up to 10 colors plus an angle in degrees
    struct Gradient {
        std::vector<Color> colors;
        double             angleDeg = 0;

        Gradient() = default;
        Gradient(Color c) : colors{c} {}
        Gradient(std::vector<Color> cs, double angle = 0) : colors(std::move(cs)), angleDeg(angle) {}
    };

    inline Gradient gradient(std::vector<Color> colors, double angleDeg = 0) {
        return Gradient{std::move(colors), angleDeg};
    }

    struct Vec2 {
        float x = 0, y = 0;
    };

    // CSS-style gap: 1, 2 (vertical, horizontal) or 4 (top, right, bottom, left) values
    struct Gap {
        int64_t top = 0, right = 0, bottom = 0, left = 0;

        constexpr Gap() = default;
        constexpr Gap(int64_t all) : top(all), right(all), bottom(all), left(all) {}
        constexpr Gap(int64_t vertical, int64_t horizontal) : top(vertical), right(horizontal), bottom(vertical), left(horizontal) {}
        constexpr Gap(int64_t t, int64_t r, int64_t b, int64_t l) : top(t), right(r), bottom(b), left(l) {}
    };

    // 700 or "bold"
    struct FontWeight {
        std::variant<int64_t, std::string> v;

        FontWeight(int w) : v(int64_t(w)) {}
        FontWeight(int64_t w) : v(w) {}
        FontWeight(const char* name) : v(std::string(name)) {}
        FontWeight(std::string name) : v(std::move(name)) {}
    };

    // for integer options that also accept named values (e.g. general.resize_corner = "top_left")
    struct IntChoice {
        std::variant<int64_t, std::string> v;

        IntChoice(int w) : v(int64_t(w)) {}
        IntChoice(int64_t w) : v(w) {}
        IntChoice(bool b) : v(int64_t(b ? 1 : 0)) {}
        IntChoice(const char* name) : v(std::string(name)) {}
        IntChoice(std::string name) : v(std::move(name)) {}
    };

    // ---- modifiers ----

    struct Mods {
        uint32_t mask = 0;
        constexpr Mods operator|(Mods o) const {
            return {mask | o.mask};
        }
        constexpr bool operator==(const Mods&) const = default;
    };

    // values equal the compositor's modifier bits (checked by a static_assert in the implementation)
    inline constexpr Mods NONE{0};
    inline constexpr Mods SHIFT{1 << 0};
    inline constexpr Mods CAPS{1 << 1};
    inline constexpr Mods CTRL{1 << 2};
    inline constexpr Mods ALT{1 << 3};
    inline constexpr Mods MOD2{1 << 4};
    inline constexpr Mods MOD3{1 << 5};
    inline constexpr Mods SUPER{1 << 6};
    inline constexpr Mods MOD5{1 << 7};

    // ---- option wrapper ----

    namespace detail {
        template <typename T, typename U>
        inline constexpr bool optAssignable = [] {
            using D = std::decay_t<U>;
            if constexpr (std::is_same_v<D, T>)
                return true;
            else if constexpr (std::is_same_v<T, bool>)
                return false; // only bool: no pointer / integer accidents
            else if constexpr (std::is_integral_v<T>)
                return std::is_integral_v<D> && !std::is_same_v<D, bool>;
            else if constexpr (std::is_floating_point_v<T>)
                return std::is_arithmetic_v<D> && !std::is_same_v<D, bool>;
            else if constexpr (std::is_same_v<T, std::string>)
                return std::is_constructible_v<std::string, U> && !std::is_arithmetic_v<D> && !std::is_same_v<D, std::nullptr_t>;
            else
                return std::is_convertible_v<U, T>;
        }();
    }

    // A config leaf that remembers whether it was assigned.
    template <typename T>
    struct Opt {
        std::optional<T> value;

        Opt& operator=(const T& x) {
            value = x;
            return *this;
        }
        Opt& operator=(T&& x) {
            value = std::move(x);
            return *this;
        }
        template <typename U>
            requires(!std::is_same_v<std::decay_t<U>, T> && !std::is_same_v<std::decay_t<U>, Opt> && detail::optAssignable<T, U>)
        Opt& operator=(U&& x) {
            if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>)
                value = static_cast<T>(x);
            else
                value = T(std::forward<U>(x));
            return *this;
        }

        bool isSet() const {
            return value.has_value();
        }
        const T& get() const {
            return *value;
        }
        void unset() {
            value.reset();
        }
    };

} // namespace abyss
