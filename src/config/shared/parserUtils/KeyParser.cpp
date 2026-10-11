#include "KeyParser.hpp"

#include <optional>
#include <vector>

#include <xkbcommon/xkbcommon.h>

#include <abyssutils/string/Numeric.hpp>
#include <abyssutils/string/String.hpp>
#include <abyssutils/string/VarList2.hpp>

#include "../../../devices/IKeyboard.hpp"

using namespace Abyssutils::String;


static std::optional<eKeyboardModifiers> modFromSv(std::string_view sv) {
    if (sv == "SHIFT")
        return HL_MODIFIER_SHIFT;
    if (sv == "CAPS")
        return HL_MODIFIER_CAPS;
    if (sv == "CTRL" || sv == "CONTROL")
        return HL_MODIFIER_CTRL;
    if (sv == "ALT" || sv == "MOD1")
        return HL_MODIFIER_ALT;
    if (sv == "MOD2")
        return HL_MODIFIER_MOD2;
    if (sv == "MOD3")
        return HL_MODIFIER_MOD3;
    if (sv == "SUPER" || sv == "WIN" || sv == "LOGO" || sv == "MOD4" || sv == "META")
        return HL_MODIFIER_META;
    if (sv == "MOD5")
        return HL_MODIFIER_MOD5;

    return std::nullopt;
}

static bool isSymSpecial(std::string_view sv) {
    if (sv == "mouse_down" || sv == "mouse_up" || sv == "mouse_left" || sv == "mouse_right")
        return true;

    return sv.starts_with("switch:") || sv.starts_with("mouse:");
}

std::expected<void, std::string> Config::KeyParser::parseKeyString(SKeybind& kb, std::string_view sv) {
    bool                                                modsEnded = false, specialSym = false;
    CVarList2                                           vl(sv, 0, '+', true);

    uint32_t                                            modMask = 0;
    std::vector<std::pair<xkb_keysym_t, xkb_keycode_t>> keysyms;
    std::string                                         lastKeyArg;

    if (sv == "catchall") {
        kb.catchAll = true;
        return {};
    }

    for (const auto& a : vl) {
        auto arg = Abyssutils::String::trim(a);

        auto mask = modFromSv(arg);

        if (!mask)
            modsEnded = true;

        if (modsEnded && mask)
            return std::unexpected("Modifiers must come first in the list");

        if (mask) {
            modMask |= *mask;
            continue;
        }

        if (specialSym)
            return std::unexpected("Cannot combine special syms (e.g. mouse_down + Q)");

        if (isSymSpecial(arg)) {
            if (!keysyms.empty())
                return std::unexpected("Cannot combine special syms (e.g. mouse_down + Q)");

            specialSym = true;
            kb.key     = arg;
            continue;
        }

        if (arg.starts_with("code:") && isNumber(std::string{arg.substr(5)})) {
            auto res = strToNumber<uint32_t>(arg.substr(5));

            if (!res)
                return std::unexpected(std::format("Invalid keycode: \"{}\".", arg));

            keysyms.emplace_back(XKB_KEY_NoSymbol, xkb_keycode_t{*res});
            continue;
        }

        auto sym = xkb_keysym_from_name(std::string{arg}.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);

        if (sym == XKB_KEY_NoSymbol) {
            if (arg.contains(' '))
                return std::unexpected(std::format("Unknown keysym: \"{}\", did you forget a +?", arg));

            if (arg == "Enter")
                return std::unexpected(std::format(R"(Unknown keysym: "{}", did you mean "Return"?)", arg));

            return std::unexpected(std::format("Unknown keysym: \"{}\"", arg));
        }

        lastKeyArg = arg;
        keysyms.emplace_back(sym, 0);
    }

    kb.modmask = modMask;
    kb.sMkKeys = std::move(keysyms);
    if (!specialSym && !lastKeyArg.empty())
        kb.key = lastKeyArg;
    return {};
}
uint32_t Config::KeyParser::modFromName(std::string_view sv) {
    const auto M = modFromSv(sv);
    return M ? sc<uint32_t>(*M) : 0;
}
