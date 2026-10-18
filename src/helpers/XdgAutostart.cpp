#include "XdgAutostart.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>

namespace fs = std::filesystem;

static std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
        s.remove_suffix(1);
    return s;
}

static std::vector<std::string> split(std::string_view s, char sep) {
    std::vector<std::string> result;
    while (!s.empty()) {
        const auto POS  = s.find(sep);
        const auto PART = trim(s.substr(0, POS));
        if (!PART.empty())
            result.emplace_back(PART);
        if (POS == std::string_view::npos)
            break;
        s.remove_prefix(POS + 1);
    }
    return result;
}

std::optional<XdgAutostart::SEntry> XdgAutostart::parse(std::string_view content) {
    SEntry entry;
    bool   inEntry = false, found = false;

    std::istringstream stream{std::string{content}};
    std::string        rawLine;
    while (std::getline(stream, rawLine)) {
        const auto LINE = trim(rawLine);
        if (LINE.empty() || LINE.front() == '#')
            continue;

        if (LINE.front() == '[') {
            inEntry = LINE == "[Desktop Entry]";
            found |= inEntry;
            continue;
        }

        if (!inEntry)
            continue;

        const auto EQ = LINE.find('=');
        if (EQ == std::string_view::npos)
            continue;

        // localized keys (Name[de]=...) are irrelevant here
        const auto KEY   = trim(LINE.substr(0, EQ));
        const auto VALUE = trim(LINE.substr(EQ + 1));
        if (KEY == "Exec")
            entry.exec = VALUE;
        else if (KEY == "TryExec")
            entry.tryExec = VALUE;
        else if (KEY == "OnlyShowIn")
            entry.onlyShowIn = split(VALUE, ';');
        else if (KEY == "NotShowIn")
            entry.notShowIn = split(VALUE, ';');
        else if (KEY == "Hidden")
            entry.hidden = VALUE == "true";
        else if (KEY == "X-GNOME-Autostart-enabled")
            entry.enabled = VALUE != "false";
        else if (KEY == "Type")
            entry.application = VALUE == "Application";
    }

    if (!found)
        return std::nullopt;
    return entry;
}

std::string XdgAutostart::stripFieldCodes(std::string_view exec) {
    std::string result;
    result.reserve(exec.size());
    for (size_t i = 0; i < exec.size(); ++i) {
        if (exec[i] != '%') {
            result += exec[i];
            continue;
        }

        // %% is a literal %, every other code (%f %F %u %U %i %c %k ...) has nothing to expand at autostart
        if (i + 1 < exec.size() && exec[i + 1] == '%')
            result += '%';
        ++i;
    }
    return std::string{trim(result)};
}

std::string XdgAutostart::program(std::string_view exec) {
    exec = trim(exec);
    if (exec.empty())
        return "";

    // a quoted program may contain spaces
    std::string_view first;
    if (exec.front() == '"' || exec.front() == '\'') {
        const auto CLOSE = exec.find(exec.front(), 1);
        first            = exec.substr(1, CLOSE == std::string_view::npos ? std::string_view::npos : CLOSE - 1);
    } else
        first = exec.substr(0, exec.find(' '));

    return fs::path{first}.filename().string();
}

bool XdgAutostart::shownIn(const SEntry& entry, std::string_view currentDesktops) {
    const auto DESKTOPS = split(currentDesktops, ':');
    auto       anyOf    = [&DESKTOPS](const std::vector<std::string>& list) {
        return std::ranges::any_of(list, [&DESKTOPS](const std::string& d) { return std::ranges::find(DESKTOPS, d) != DESKTOPS.end(); });
    };

    if (anyOf(entry.notShowIn))
        return false;
    if (!entry.onlyShowIn.empty())
        return anyOf(entry.onlyShowIn);
    return true;
}

bool XdgAutostart::wanted(const SEntry& entry) {
    return !entry.hidden && entry.enabled && entry.application && !stripFieldCodes(entry.exec).empty();
}

std::vector<std::pair<std::string, XdgAutostart::SEntry>> XdgAutostart::collect(const std::vector<fs::path>& dirs) {
    // a hidden user file still hides the system file of the same name, so remember every name
    std::map<std::string, std::optional<SEntry>> byName;

    for (const auto& dir : dirs) {
        std::error_code ec;
        for (const auto& file : fs::directory_iterator(dir, ec)) {
            if (file.path().extension() != ".desktop")
                continue;

            const auto NAME = file.path().filename().string();
            if (byName.contains(NAME))
                continue;

            std::ifstream     in(file.path());
            std::stringstream content;
            content << in.rdbuf();
            byName[NAME] = parse(content.str());
        }
    }

    std::vector<std::pair<std::string, SEntry>> result;
    for (auto& [name, entry] : byName) {
        if (!entry)
            continue;
        result.emplace_back(name, std::move(*entry));
    }
    return result;
}

std::vector<fs::path> XdgAutostart::directories() {
    std::vector<fs::path> dirs;

    if (const auto CONFIG = getenv("XDG_CONFIG_HOME"); CONFIG && *CONFIG)
        dirs.emplace_back(fs::path{CONFIG} / "autostart");
    else if (const auto HOME = getenv("HOME"); HOME && *HOME)
        dirs.emplace_back(fs::path{HOME} / ".config" / "autostart");

    const auto  CONFIGDIRS = getenv("XDG_CONFIG_DIRS");
    const auto  SYSTEMDIRS = split(CONFIGDIRS && *CONFIGDIRS ? CONFIGDIRS : "/etc/xdg", ':');
    for (const auto& dir : SYSTEMDIRS)
        dirs.emplace_back(fs::path{dir} / "autostart");

    return dirs;
}
