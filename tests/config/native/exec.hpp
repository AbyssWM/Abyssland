#include <abyss/config.hpp>
#include "managers/KeybindManager.hpp"
#include <cstdlib>
#include <string>

void configure(abyss::Config& c) {
    using namespace abyss;
    c.monitor("", "preferred", "auto", 1.0);
    c.misc.disable_abyssland_logo   = true;
    c.misc.disable_splash_rendering = true;
    c.cursor.theme                  = "Sweet-cursors";
    c.cursor.size                   = 32;
    const auto* directory           = std::getenv("ABYSS_EXEC_TEST_DIR");
    if (!directory)
        return;
    const std::string root = directory;
    Action            callback{[root] { return exec("printf '%s\\n' \"$((6 * 7))\" | cat >> '" + root + "/bind'"); }};
    c.bind(SUPER, "F12", callback);
    c.onStart([root] { exec("printf 'started\\n' >> '" + root + "/start'"); });
    c.exec("printf 'compat\\n' >> '" + root + "/compat'");
    c.onShutdown([root] { exec("printf 'stopped\\n' >> '" + root + "/shutdown'"); });
    g_pKeybindManager->m_dispatchers["test_native"] = [](std::string) {
        // Exercise the callback stored by bind(), not a separate replacement.
        for (const auto& bind : g_pKeybindManager->m_keybinds) {
            if (bind->key == "F12" && bind->nativeFn)
                return bind->nativeFn();
        }
        return error("test bind not found");
    };
    g_pKeybindManager->m_dispatchers["test_run"]    = [root](std::string) { return run(Action{[root] { return exec("printf 'run\\n' >> '" + root + "/run'"); }}); };
    g_pKeybindManager->m_dispatchers["test_cursor"] = [](std::string value) {
        if (value == "restore") {
            set("cursor:theme", "Sweet-cursors");
            set("cursor:size", 32);
        } else {
            set("cursor:theme", "default");
            set("cursor:size", 24);
        }
        return ok();
    };
}
