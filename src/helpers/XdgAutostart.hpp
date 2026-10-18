#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// XDG autostart (https://specifications.freedesktop.org/autostart-spec/latest/): what full desktops
// start from ~/.config/autostart and /etc/xdg/autostart. With systemd, xdg-desktop-autostart.target
// does this; without it, Abyssland does it through SessionServices.
namespace XdgAutostart {
    struct SEntry {
        std::string              exec;
        std::string              tryExec;
        std::vector<std::string> onlyShowIn;
        std::vector<std::string> notShowIn;
        bool                     hidden      = false;
        bool                     enabled     = true; // X-GNOME-Autostart-enabled
        bool                     application = true; // Type=Application
    };

    // the [Desktop Entry] group of a .desktop file, nullopt without one
    std::optional<SEntry> parse(std::string_view content);

    // Exec without field codes (%f, %U, ...); %% becomes %
    std::string stripFieldCodes(std::string_view exec);

    // the program an Exec line starts, without its directory
    std::string program(std::string_view exec);

    // whether the entry is meant for one of the desktops in XDG_CURRENT_DESKTOP (":"-separated)
    bool shownIn(const SEntry& entry, std::string_view currentDesktops);

    // whether the entry wants to be started at all (not hidden, not disabled, an application with an Exec)
    bool wanted(const SEntry& entry);

    // entries from the given directories, in priority order: a file name found earlier hides the
    // same name in later directories. Sorted by file name.
    std::vector<std::pair<std::string, SEntry>> collect(const std::vector<std::filesystem::path>& dirs);

    // $XDG_CONFIG_HOME/autostart, then each $XDG_CONFIG_DIRS/autostart
    std::vector<std::filesystem::path> directories();
}
