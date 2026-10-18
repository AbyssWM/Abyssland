#pragma once

// The per-user build of `abyssctl reload`: ~/.config/abyss/.build. Hidden, so the config directory
// only shows the user's own files. Holds the CMake tree, the cloned sources and the logs.

#include <cstdlib>
#include <filesystem>

namespace UserBuild {
    // $XDG_CONFIG_HOME/abyss, or ~/.config/abyss
    inline std::filesystem::path configDir() {
        if (const auto XDG = getenv("XDG_CONFIG_HOME"); XDG && *XDG)
            return std::filesystem::path{XDG} / "abyss";
        const auto HOME = getenv("HOME");
        return std::filesystem::path{HOME ? HOME : ""} / ".config" / "abyss";
    }

    inline std::filesystem::path buildDir() {
        return configDir() / ".build";
    }

    inline std::filesystem::path sourceDir() {
        return buildDir() / "src";
    }
}
