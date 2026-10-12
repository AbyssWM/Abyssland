#include "Abysspaper.hpp"
#include "../helpers/Memory.hpp"

#include <optional>
#include <format>
#include <filesystem>
#include <print>

#include <abysspaper_core-client.hpp>

#include <abyssutils/string/VarList2.hpp>
using namespace Abyssutils::String;

using namespace std::string_literals;

constexpr const char*          SOCKET_NAME = ".abysspaper.sock";
static SP<CCAbysspaperCoreImpl> g_coreImpl;

constexpr const uint32_t       PROTOCOL_VERSION_SUPPORTED = 2;

//
static abysspaperCoreWallpaperFitMode fitFromString(const std::string_view& sv) {
    if (sv == "contain")
        return ABYSSPAPER_CORE_WALLPAPER_FIT_MODE_CONTAIN;
    if (sv == "fit" || sv == "stretch")
        return ABYSSPAPER_CORE_WALLPAPER_FIT_MODE_STRETCH;
    if (sv == "tile")
        return ABYSSPAPER_CORE_WALLPAPER_FIT_MODE_TILE;
    return ABYSSPAPER_CORE_WALLPAPER_FIT_MODE_COVER;
}

static std::expected<std::string, std::string> resolvePath(const std::string_view& sv) {
    std::error_code ec;
    auto            can = std::filesystem::canonical(sv, ec);

    if (ec)
        return std::unexpected(std::format("invalid path: {}", ec.message()));

    return can;
}

static std::expected<std::string, std::string> getFullPath(const std::string_view& sv) {
    if (sv.empty())
        return std::unexpected("empty path");

    if (sv[0] == '~') {
        static auto HOME = getenv("HOME");
        if (!HOME || HOME[0] == '\0')
            return std::unexpected("home path but no $HOME");

        return resolvePath(std::string{HOME} + "/"s + std::string{sv.substr(1)});
    }

    return resolvePath(sv);
}

static std::expected<void, std::string> doWallpaper(const std::string_view& RHS) {
    CVarList2         args(std::string{RHS}, 0, ',');

    const std::string MONITOR  = std::string{args[0]};
    const auto&       PATH_RAW = args[1];
    const auto&       FIT      = args[2];

    if (PATH_RAW.empty())
        return std::unexpected("not enough args");

    const auto RTDIR = getenv("XDG_RUNTIME_DIR");

    if (!RTDIR || RTDIR[0] == '\0')
        return std::unexpected("can't send: no XDG_RUNTIME_DIR");

    const auto HIS = getenv("ABYSSLAND_INSTANCE_SIGNATURE");

    if (!HIS || HIS[0] == '\0')
        return std::unexpected("can't send: no ABYSSLAND_INSTANCE_SIGNATURE (not running under abyssland)");

    const auto PATH = getFullPath(PATH_RAW);

    if (!PATH)
        return std::unexpected(std::format("bad path: {}", PATH_RAW));

    auto socketPath = RTDIR + "/abyss/"s + HIS + "/"s + SOCKET_NAME;

    auto socket = Abysswire::IClientSocket::open(socketPath);

    if (!socket)
        return std::unexpected("can't send: failed to connect to abysspaper (is it running?)");

    g_coreImpl = makeShared<CCAbysspaperCoreImpl>(PROTOCOL_VERSION_SUPPORTED);

    socket->addImplementation(g_coreImpl);

    if (!socket->waitForHandshake())
        return std::unexpected("can't send: wire handshake failed");

    auto spec = socket->getSpec(g_coreImpl->protocol()->specName());

    if (!spec)
        return std::unexpected("can't send: abysspaper doesn't have the spec?!");

    auto manager = makeShared<CCAbysspaperCoreManagerObject>(socket->bindProtocol(g_coreImpl->protocol(), std::min(PROTOCOL_VERSION_SUPPORTED, spec->specVer())));

    if (!manager)
        return std::unexpected("wire error: couldn't create manager");

    auto wallpaper = makeShared<CCAbysspaperWallpaperObject>(manager->sendGetWallpaperObject());

    if (!wallpaper)
        return std::unexpected("wire error: couldn't create wallpaper object");

    bool                       canExit = false;
    std::optional<std::string> err;

    wallpaper->setFailed([&canExit, &err](uint32_t code) {
        canExit = true;
        switch (code) {
            case ABYSSPAPER_CORE_APPLYING_ERROR_INVALID_PATH: err = std::format("failed to set wallpaper: Invalid path", code); break;
            case ABYSSPAPER_CORE_APPLYING_ERROR_INVALID_MONITOR: err = std::format("failed to set wallpaper: Invalid monitor", code); break;
            default: err = std::format("failed to set wallpaper: unknown error, code {}", code); break;
        }
    });
    wallpaper->setSuccess([&canExit]() { canExit = true; });

    wallpaper->sendPath(PATH->c_str());
    wallpaper->sendMonitorName(MONITOR.c_str());
    if (!FIT.empty())
        wallpaper->sendFitMode(fitFromString(FIT));

    wallpaper->sendApply();

    while (!canExit) {
        socket->dispatchEvents(true);
    }

    if (err)
        return std::unexpected(*err);

    return {};
}

static std::expected<void, std::string> doListActive() {
    const auto RTDIR = getenv("XDG_RUNTIME_DIR");

    if (!RTDIR || RTDIR[0] == '\0')
        return std::unexpected("can't send: no XDG_RUNTIME_DIR");

    const auto HIS = getenv("ABYSSLAND_INSTANCE_SIGNATURE");

    if (!HIS || HIS[0] == '\0')
        return std::unexpected("can't send: no ABYSSLAND_INSTANCE_SIGNATURE (not running under abyssland)");

    auto socketPath = RTDIR + "/abyss/"s + HIS + "/"s + SOCKET_NAME;

    auto socket = Abysswire::IClientSocket::open(socketPath);

    if (!socket)
        return std::unexpected("can't send: failed to connect to abysspaper (is it running?)");

    g_coreImpl = makeShared<CCAbysspaperCoreImpl>(PROTOCOL_VERSION_SUPPORTED);

    socket->addImplementation(g_coreImpl);

    if (!socket->waitForHandshake())
        return std::unexpected("can't send: wire handshake failed");

    auto spec = socket->getSpec(g_coreImpl->protocol()->specName());

    if (!spec)
        return std::unexpected("can't send: abysspaper doesn't have the spec?!");

    if (spec->specVer() < 2)
        return std::unexpected("can't send: abysspaper protocol version too low (abysspaper too old)");

    auto manager = makeShared<CCAbysspaperCoreManagerObject>(socket->bindProtocol(g_coreImpl->protocol(), std::min(PROTOCOL_VERSION_SUPPORTED, spec->specVer())));

    if (!manager)
        return std::unexpected("wire error: couldn't create manager");

    auto status = makeShared<CCAbysspaperStatusObject>(manager->sendGetStatusObject());

    status->setActiveWallpaper([](const char* mon, const char* wp) { std::println("{}: {}", mon, wp); });

    socket->roundtrip();

    return {};
}

std::expected<void, std::string> Abysspaper::makeAbysspaperRequest(const std::string_view& rq) {
    if (!rq.contains(' '))
        return std::unexpected("Invalid request");

    if (!rq.starts_with("/abysspaper "))
        return std::unexpected("Invalid request");

    std::string_view LHS, RHS;
    auto             spacePos = rq.find(' ', 12);
    LHS                       = rq.substr(12, spacePos - 12) /* strlen("/abysspaper ") */;
    RHS                       = rq.substr(spacePos + 1);

    if (LHS == "wallpaper")
        return doWallpaper(RHS);
    else if (LHS == "listactive")
        return doListActive();
    else
        return std::unexpected("invalid abysspaper request");

    return {};
}
