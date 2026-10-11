#include "Nix.hpp"

#include "Logger.hpp"
#include "../core/State.hpp"

#include <filesystem>
#include <algorithm>
#include <abyssutils/string/VarList2.hpp>
#include <abyssutils/string/String.hpp>
#include <abyssutils/os/Process.hpp>
#include <abyssutils/os/File.hpp>

#include <glaze/glaze.hpp>

using namespace Abyssutils::String;
using namespace Abyssutils::OS;

using namespace Abyssutils::File;

static std::optional<std::string> getFromEtcOsRelease(const std::string_view& sv) {
    static std::string content = "";
    static bool        once    = true;

    if (once) {
        once = false;

        auto read = readFileAsString("/etc/os-release");
        content   = read.value_or("");
    }

    static CVarList2 vars(std::move(content), 0, '\n', true);

    for (const auto& v : vars) {
        if (v.starts_with(sv) && v.contains('=')) {
            // found
            auto value = trim(v.substr(v.find('=') + 1));

            if (value.back() == value.front() && value.back() == '"')
                value = value.substr(1, value.size() - 2);

            return std::string{value};
        }
    }

    return std::nullopt;
}

static bool executableExistsInPath(const std::string& exe) {
    const char* PATHENV = std::getenv("PATH");
    if (!PATHENV)
        return false;

    CVarList2       paths(PATHENV, 0, ':', true);
    std::error_code ec;

    for (const auto& PATH : paths) {
        std::filesystem::path candidate = std::filesystem::path(PATH) / exe;
        if (!std::filesystem::exists(candidate, ec) || ec)
            continue;
        if (!std::filesystem::is_regular_file(candidate, ec) || ec)
            continue;
        auto perms = std::filesystem::status(candidate, ec).permissions();
        if (ec)
            continue;
        if ((perms & std::filesystem::perms::others_exec) != std::filesystem::perms::none)
            return true;
    }

    return false;
}

std::expected<void, std::string> Nix::nixEnvironmentOk() {
    if (!shouldUseNixGL())
        return {};

    if (!executableExistsInPath("nixGL"))
        return std::unexpected(
            "Abyssland was installed using Nix, but you're not on NixOS. This requires nixGL to be installed as well.\nYou can install nixGL by running \"nix profile install "
            "github:guibou/nixGL --impure\" in your terminal.");

    return {};
}

bool Nix::shouldUseNixGL() {
    if (g_state->forceNixGl)
        return true;

    if (g_state->noNixGl)
        return false;

    // check if installed abyssland is nix'd
    CProcess proc(ABYSSLAND_BINARY, {"--version-json"});
    if (!proc.runSync()) {
        g_logger->log(Abyssutils::CLI::LOG_ERR, "failed to obtain abyssland version string");
        return false;
    }

    auto json = glz::read_json<glz::generic>(proc.stdOut());
    if (!json) {
        g_logger->log(Abyssutils::CLI::LOG_ERR, "failed to obtain abyssland version string (bad json)");
        return false;
    }

    const auto FLAGS  = (*json)["flags"].get_array();
    const bool IS_NIX = std::ranges::any_of(FLAGS, [](const auto& e) { return e.get_string() == std::string_view{"nix"}; });

    if (IS_NIX) {
        const auto NAME = getFromEtcOsRelease("NAME");

        // Abyssland is nix: recommend nixGL iff !NIX && !NIX-OPENGL

        if (*NAME == "NixOS")
            return false;

        std::error_code ec;

        if (std::filesystem::exists("/run/opengl-driver", ec) && !ec) // NOLINTNEXTLINE
            return false;

        // we are not on nix / no nix opengl driver
        return true;
    }

    return false;
}
