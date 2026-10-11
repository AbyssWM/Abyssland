// The compiled-in user config. ABYSS_USER_CONFIG (set by CMake for this file only) is the path of the user's config.hpp,
// by default ~/.config/abyss/config.hpp. Only <abyss/config.hpp> is visible here, on purpose: this is the world the user writes in.
#include <abyss/config.hpp>

#include ABYSS_USER_CONFIG

void abyss::detail::userConfigure(abyss::Config& c) {
    ::configure(c);
}
