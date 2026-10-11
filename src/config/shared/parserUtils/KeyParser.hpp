#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "../../../managers/KeybindManager.hpp"

namespace Config::KeyParser {
    // Parses a key string like "SUPER+SHIFT+Q", "code:10", "mouse:272", "mouse_down", "switch:on:Lid Switch" or "catchall"
    // into kb.modmask / kb.sMkKeys / kb.key / kb.catchAll.
    // Modifiers: SHIFT, CAPS, CTRL/CONTROL, ALT/MOD1, MOD2, MOD3, SUPER/WIN/LOGO/MOD4/META, MOD5. They must come first.
    std::expected<void, std::string> parseKeyString(SKeybind& kb, std::string_view sv);

    // modifier name -> mask (0 if not a modifier)
    uint32_t modFromName(std::string_view sv);
}
