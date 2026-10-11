#include "SystemInfo.hpp"

#include "../Compositor.hpp"
#include "../version.h"
#include "../render/OpenGL.hpp"
#include "../config/ConfigManager.hpp"
#include "../state/MonitorState.hpp"

#include <abyssutils/string/String.hpp>

#include <sys/utsname.h>
#include <fstream>
#include <sstream>
#include <format>
#include <algorithm>

using namespace Helpers::SystemInfo;
using namespace Helpers;
using namespace Abyssutils::String;
using namespace Render::GL;

static void trimTrailingComma(std::string& str) {
    if (!str.empty() && str.back() == ',')
        str.pop_back();
}

std::string SystemInfo::getStatus(eAbyssCtlOutputFormat fmt) {
    Abyssal::eBackendType backendType = Abyssal::eBackendType::ABYSSAL_BACKEND_NULL;

    for (const auto& i : g_pCompositor->m_aqBackend->getImplementations()) {
        if (i->type() == Abyssal::eBackendType::ABYSSAL_BACKEND_NULL || i->type() == Abyssal::eBackendType::ABYSSAL_BACKEND_HEADLESS)
            continue;

        backendType = i->type();
        break;
    }

    std::string backendStr;

    switch (backendType) {
        case Abyssal::ABYSSAL_BACKEND_DRM: backendStr = "drm"; break;
        case Abyssal::ABYSSAL_BACKEND_WAYLAND: backendStr = "wayland"; break;
        default: backendStr = "error"; break;
    }

    if (fmt == eAbyssCtlOutputFormat::FORMAT_JSON) {

        return std::format(R"#(
{{
    "configProvider": "{}",
    "backend": "{}"
}}
)#",
                           "native", backendStr);
    }

    return std::format(R"#(
configProvider: {}
backend: {}
)#",
                       "native", backendStr);
}

std::string SystemInfo::getVersion(eAbyssCtlOutputFormat fmt) {

    auto commitMsg = trim(GIT_COMMIT_MESSAGE);
    std::ranges::replace(commitMsg, '#', ' ');

    if (fmt == eAbyssCtlOutputFormat::FORMAT_NORMAL) {
        std::string result = std::format("Abyssland {} built from branch {} at commit {} {} ({}).\n"
                                         "Date: {}\n"
                                         "Tag: {}, commits: {}\n",
                                         ABYSSLAND_VERSION, GIT_BRANCH, GIT_COMMIT_HASH, GIT_DIRTY, commitMsg, GIT_COMMIT_DATE, GIT_TAG, GIT_COMMITS);

        result += "\n";
        result += getBuiltSystemLibraryNames();
        result += "\n";
        result += "Version ABI string: ";
        result += GIT_COMMIT_HASH;
        result += "\n";

#if (!ISDEBUG && !defined(NO_XWAYLAND) && !defined(BUILT_WITH_NIX))
        result += "no flags were set\n";
#else
        result += "flags set:\n";
#if ISDEBUG
        result += "debug\n";
#endif
#ifdef NO_XWAYLAND
        result += "no xwayland\n";
#endif
#ifdef BUILT_WITH_NIX
        result += "nix\n";
#endif
#endif
        return result;
    } else {
        std::string result = std::format(
            R"#({{
    "branch": "{}",
    "commit": "{}",
    "version": "{}",
    "dirty": {},
    "commit_message": "{}",
    "commit_date": "{}",
    "tag": "{}",
    "commits": "{}",
    "buildAbyssal": "{}",
    "buildAbyssutils": "{}",
    "buildAbysscursor": "{}",
    "buildAbyssgraphics": "{}",
    "systemAbyssal": "{}",
    "systemAbyssutils": "{}",
    "systemAbysscursor": "{}",
    "systemAbyssgraphics": "{}",
    "abiHash": "{}",
    "flags": [)#",
            GIT_BRANCH, GIT_COMMIT_HASH, ABYSSLAND_VERSION, (GIT_DIRTY == std::string_view{"dirty"} ? "true" : "false"), escapeJSONStrings(commitMsg), GIT_COMMIT_DATE, GIT_TAG,
            GIT_COMMITS, ABYSSAL_VERSION, ABYSSUTILS_VERSION, ABYSSCURSOR_VERSION, ABYSSGRAPHICS_VERSION, getSystemLibraryVersion("abyssal"),
            getSystemLibraryVersion("abyssutils"), getSystemLibraryVersion("abysscursor"), getSystemLibraryVersion("abyssgraphics"),
            GIT_COMMIT_HASH);

#if ISDEBUG
        result += "\"debug\",";
#endif
#ifdef NO_XWAYLAND
        result += "\"no xwayland\",";
#endif
#ifdef BUILT_WITH_NIX
        result += "\"nix\",";
#endif

        trimTrailingComma(result);

        result += "]\n}";

        return result;
    }

    return ""; // make the compiler happy
}

std::string SystemInfo::getSystemInfo() {
    std::string result = getVersion(eAbyssCtlOutputFormat::FORMAT_NORMAL);

    static auto check   = [](bool y) -> std::string { return y ? "✔️" : "❌"; };
    static auto backend = [](Abyssal::eBackendType t) -> std::string {
        switch (t) {
            case Abyssal::ABYSSAL_BACKEND_DRM: return "drm";
            case Abyssal::ABYSSAL_BACKEND_HEADLESS: return "headless";
            case Abyssal::ABYSSAL_BACKEND_WAYLAND: return "wayland";
            default: break;
        }
        return "?";
    };

    result += "\n\nSystem Information:\n";

    struct utsname unameInfo;

    uname(&unameInfo);

    result += "System name: " + std::string{unameInfo.sysname} + "\n";
    result += "Node name: " + std::string{unameInfo.nodename} + "\n";
    result += "Release: " + std::string{unameInfo.release} + "\n";
    result += "Version: " + std::string{unameInfo.version} + "\n";
    result += "\n";
    result += getBuiltSystemLibraryNames();
    result += "\n";

    result += "\n\n";

#if defined(__DragonFly__) || defined(__FreeBSD__)
    const std::string GPUINFO = execAndGet("pciconf -lv | grep -F -A4 vga");
#elif defined(__arm__) || defined(__aarch64__)
    std::string                 GPUINFO;
    const std::filesystem::path dev_tree = "/proc/device-tree";
    try {
        if (std::filesystem::exists(dev_tree) && std::filesystem::is_directory(dev_tree)) {
            std::for_each(std::filesystem::directory_iterator(dev_tree), std::filesystem::directory_iterator{}, [&](const std::filesystem::directory_entry& entry) {
                if (std::filesystem::is_directory(entry) && entry.path().filename().string().starts_with("soc")) {
                    std::for_each(std::filesystem::directory_iterator(entry.path()), std::filesystem::directory_iterator{}, [&](const std::filesystem::directory_entry& sub_entry) {
                        if (std::filesystem::is_directory(sub_entry) && sub_entry.path().filename().string().starts_with("gpu")) {
                            std::filesystem::path file_path = sub_entry.path() / "compatible";
                            std::ifstream         file(file_path);
                            if (file)
                                GPUINFO.append(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
                        }
                    });
                }
            });
        }
    } catch (...) { GPUINFO = "error"; }
#else
    const std::string GPUINFO = execAndGet("lspci -vnn | grep -E '(VGA|Display|3D)'");
#endif
    result += "GPU information: \n" + GPUINFO;
    if (GPUINFO.contains("NVIDIA") && std::filesystem::exists("/proc/driver/nvidia/version")) {
        std::ifstream file("/proc/driver/nvidia/version");
        std::string   line;
        if (file.is_open()) {
            while (std::getline(file, line)) {
                if (!line.contains("NVRM"))
                    continue;
                result += line;
                result += "\n";
            }
        } else
            result += "error";
    }
    result += "\n\n";

    if (std::ifstream file("/etc/os-release"); file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        result += "os-release: " + buffer.str() + "\n\n";
    } else
        result += "os-release: error\n\n";

    if (g_pAbyssOpenGL) {
        result += std::format("\nExplicit sync: {}", g_pAbyssOpenGL->m_exts.EGL_ANDROID_native_fence_sync_ext ? "supported" : "missing");
        result += std::format("\nGL ver: {}", g_pAbyssOpenGL->m_eglContextVersion == CAbyssOpenGLImpl::EGL_CONTEXT_GLES_3_2 ? "3.2" : "3.0");
    }

    if (g_pCompositor) {
        result += std::format("\nBackend: {}", g_pCompositor->m_aqBackend->hasSession() ? "drm" : "sessionless");

        result += "\n\nMonitor info:";

        for (const auto& m : State::monitorState()->monitors()) {
            result += std::format("\n\tPanel {}: {}x{}, {} {} {} {} -> backend {}\n\t\texplicit {}\n\t\tedid:\n\t\t\thdr {}\n\t\t\tchroma {}\n\t\t\tbt2020 {}\n\t\tvrr capable "
                                  "{}\n\t\tnon-desktop {}\n\t\t",
                                  m->m_name, sc<int>(m->m_pixelSize.x), sc<int>(m->m_pixelSize.y), m->m_output->name, m->m_output->make, m->m_output->model, m->m_output->serial,
                                  backend(m->m_output->getBackend()->type()), check(m->m_output->supportsExplicit), check(m->m_output->parsedEDID.hdrMetadata.has_value()),
                                  check(m->m_output->parsedEDID.chromaticityCoords.has_value()), check(m->m_output->parsedEDID.supportsBT2020), check(m->m_output->vrrCapable),
                                  check(m->m_output->nonDesktop));
        }
    }

    result += "\n\nState:\n";
    result += getStatus(FORMAT_NORMAL);

    result += "\n\n";

    return result;
}