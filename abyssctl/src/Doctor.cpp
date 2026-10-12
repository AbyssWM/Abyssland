#include "Doctor.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <regex>
#include <set>
#include <sstream>
#include <vector>
#include <unistd.h>

extern char** environ;

namespace fs = std::filesystem;

namespace {

    // ok = default, modified = deviates from the default (not an error, user's responsibility), warn = warning, info = informational only
    enum class eStatus {
        OK,
        MODIFIED,
        WARN,
        INFO,
    };

    struct SItem {
        std::string              key;
        eStatus                  status = eStatus::OK;
        std::string              summary;
        std::vector<std::string> details;
    };

    const char* statusKey(eStatus s) {
        switch (s) {
            case eStatus::OK: return "ok";
            case eStatus::MODIFIED: return "modified";
            case eStatus::WARN: return "warning";
            case eStatus::INFO: return "info";
        }
        return "info";
    }

    const char* statusLabel(eStatus s) {
        switch (s) {
            case eStatus::OK: return "ok";
            case eStatus::MODIFIED: return "modified";
            case eStatus::WARN: return "WARNING";
            case eStatus::INFO: return "info";
        }
        return "info";
    }

    // worst-wins merge: WARN > MODIFIED > INFO > OK
    int rank(eStatus s) {
        switch (s) {
            case eStatus::OK: return 0;
            case eStatus::INFO: return 1;
            case eStatus::MODIFIED: return 2;
            case eStatus::WARN: return 3;
        }
        return 0;
    }

    void raise(eStatus& cur, eStatus s) {
        if (rank(s) > rank(cur))
            cur = s;
    }

    std::string jsonEscape(std::string_view s) {
        std::string out;
        for (const char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20)
                        out += std::format("\\u{:04x}", static_cast<unsigned>(c));
                    else
                        out += c;
            }
        }
        return out;
    }

    std::string envStr(const char* name) {
        const auto E = getenv(name);
        return E ? E : "";
    }

    std::vector<std::string> split(const std::string& s, char sep) {
        std::vector<std::string> out;
        std::string              cur;
        std::stringstream        ss(s);
        while (std::getline(ss, cur, sep))
            out.push_back(cur);
        return out;
    }

    std::string lower(std::string s) {
        std::ranges::transform(s, s.begin(), [](unsigned char c) { return std::tolower(c); });
        return s;
    }

    std::string shellQuote(const std::string& s) {
        std::string out = "'";
        for (const char c : s) {
            if (c == '\'')
                out += "'\\''";
            else
                out += c;
        }
        return out + "'";
    }

    // read-only git helper; returns nullopt if git fails
    std::optional<std::string> git(const std::string& dir, const std::string& args) {
        const std::string CMD = "git -C " + shellQuote(dir) + " " + args + " 2>/dev/null";
        FILE*             p   = popen(CMD.c_str(), "r");
        if (!p)
            return std::nullopt;
        std::string out;
        char        buf[512];
        size_t      n;
        while ((n = fread(buf, 1, sizeof(buf), p)) > 0)
            out.append(buf, n);
        const int RC = pclose(p);
        if (RC != 0)
            return std::nullopt;
        while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
            out.pop_back();
        return out;
    }

    std::string readFile(const fs::path& p) {
        std::ifstream f(p, std::ios::binary);
        return std::string{std::istreambuf_iterator<char>(f), {}};
    }

    bool sameFile(const fs::path& a, const fs::path& b) {
        std::error_code ec;
        return fs::exists(a, ec) && fs::exists(b, ec) && fs::equivalent(a, b, ec) && !ec;
    }

    // crude extraction of a JSON scalar ("key":value) from the compositor's flat reply
    std::optional<std::string> jsonField(const std::string& json, const std::string& key) {
        const std::regex  RE("\"" + key + "\"\\s*:\\s*(\"([^\"]*)\"|[A-Za-z0-9_.-]+)");
        std::smatch       m;
        if (!std::regex_search(json, m, RE))
            return std::nullopt;
        return m[2].matched ? m[2].str() : m[1].str();
    }

    std::string homeDir() {
        const auto H = envStr("HOME");
        return H.empty() ? "/root" : H;
    }

    //
    // version
    //
    SItem checkVersion(const std::string& sourceDir, const std::string& instanceSig, const std::optional<std::string>& versionJson) {
        SItem it;
        it.key = "version";

#ifdef ABYSSCTL_GIT_COMMIT
        const std::string BAKEDCOMMIT = ABYSSCTL_GIT_COMMIT;
        const std::string BAKEDDIRTY  = ABYSSCTL_GIT_DIRTY;
#else
        const std::string BAKEDCOMMIT = "unknown";
        const std::string BAKEDDIRTY  = "unknown";
#endif
        it.details.push_back(std::format("abyssctl: Commit {} ({})", BAKEDCOMMIT, BAKEDDIRTY));
        if (BAKEDDIRTY == "dirty") {
            raise(it.status, eStatus::MODIFIED);
            it.details.push_back("abyssctl was built from a source tree with uncommitted changes");
        }

        // live state of the source tree (the tree `abyssctl reload` rebuilds)
        std::error_code ec;
        if (!sourceDir.empty() && fs::exists(fs::path{sourceDir} / ".git", ec)) {
            const auto HEAD = git(sourceDir, "rev-parse --short HEAD");
            const auto DIRT = git(sourceDir, "status --porcelain --untracked-files=no");
            std::optional<std::string> ahead = git(sourceDir, "rev-list --count @{upstream}..HEAD");
            std::string                against = "Upstream";
            if (!ahead) {
                ahead   = git(sourceDir, "rev-list --count origin/main..HEAD");
                against = "origin/main";
            }
            it.details.push_back(std::format("Source tree {}: HEAD {}", sourceDir, HEAD.value_or("?")));
            if (DIRT && !DIRT->empty()) {
                const auto N = std::ranges::count(*DIRT, '\n') + 1;
                raise(it.status, eStatus::MODIFIED);
                it.details.push_back(std::format("source tree has {} uncommitted change(s)", N));
            }
            if (ahead && *ahead != "0" && !ahead->empty()) {
                raise(it.status, eStatus::MODIFIED);
                it.details.push_back(std::format("{} commit(s) not on {}", *ahead, against));
            } else if (!ahead) {
                it.details.push_back("cannot compare with origin (no upstream / origin/main)");
            }
        } else {
            it.details.push_back(std::format("Source tree not found ({})", sourceDir.empty() ? "<empty>" : sourceDir));
        }

        if (versionJson) {
            const auto COMMIT = jsonField(*versionJson, "commit").value_or("?");
            const auto VER    = jsonField(*versionJson, "version").value_or("?");
            const auto BRANCH = jsonField(*versionJson, "branch").value_or("?");
            const auto DIRTY  = jsonField(*versionJson, "dirty").value_or("?");
            it.details.push_back(std::format("Instance {}: Abyssland {} branch {} commit {}{}", instanceSig, VER, BRANCH, COMMIT.substr(0, 9), DIRTY == "true" ? " (dirty)" : ""));
            if (DIRTY == "true") {
                raise(it.status, eStatus::MODIFIED);
                it.details.push_back("the running instance was built from a modified source tree");
            }
            if (BAKEDCOMMIT != "unknown" && !COMMIT.empty() && COMMIT.rfind(BAKEDCOMMIT, 0) != 0 && BAKEDCOMMIT.rfind(COMMIT.substr(0, BAKEDCOMMIT.size()), 0) != 0) {
                raise(it.status, eStatus::WARN);
                it.details.push_back("abyssctl and the running instance come from different commits");
            }
        } else {
            it.details.push_back("Instance: not reachable (no running Abyssland instance)");
            raise(it.status, eStatus::INFO);
        }

        it.summary = it.status == eStatus::OK ? "default build (committed and on origin)" :
            it.status == eStatus::MODIFIED    ? "modified build (user's responsibility)" :
            it.status == eStatus::WARN        ? "build versions do not match" :
                                                "no running instance, only abyssctl checked";
        return it;
    }

    //
    // portals
    //
    SItem checkPortals(const std::string& prefix) {
        SItem it;
        it.key = "portals";

        const auto DESKTOPENV = envStr("XDG_CURRENT_DESKTOP");
        std::vector<std::string> desktops;
        for (const auto& d : split(DESKTOPENV, ':'))
            if (!d.empty())
                desktops.push_back(lower(d));

        it.details.push_back(std::format("XDG_CURRENT_DESKTOP = {}", DESKTOPENV.empty() ? "<unset>" : DESKTOPENV));
        const bool HASABYSS = std::ranges::find(desktops, "abyssland") != desktops.end();
        if (!HASABYSS) {
            raise(it.status, eStatus::WARN);
            it.details.push_back("token 'Abyssland' missing: the abyss portal (UseIn=Abyssland) will not be selected for this session");
        }

        // search order of xdg-desktop-portal: user config, XDG_CONFIG_DIRS, /etc, XDG_DATA_DIRS, system dirs
        std::vector<std::string> dirs;
        auto                     addDir = [&dirs](const std::string& d) {
            if (!d.empty() && std::ranges::find(dirs, d) == dirs.end())
                dirs.push_back(d);
        };
        const auto XCH = envStr("XDG_CONFIG_HOME");
        addDir((XCH.empty() ? homeDir() + "/.config" : XCH) + "/xdg-desktop-portal");
        const auto XCD = envStr("XDG_CONFIG_DIRS");
        for (const auto& d : split(XCD.empty() ? "/etc/xdg" : XCD, ':'))
            addDir(d + "/xdg-desktop-portal");
        addDir("/etc/xdg-desktop-portal");
        const auto XDD = envStr("XDG_DATA_DIRS");
        for (const auto& d : split(XDD.empty() ? "/usr/local/share:/usr/share" : XDD, ':'))
            addDir(d + "/xdg-desktop-portal");
        addDir("/usr/share/xdg-desktop-portal");
        addDir(prefix + "/share/xdg-desktop-portal");

        std::error_code ec;
        std::string     winner;
        for (const auto& d : dirs) {
            for (const auto& desk : desktops) {
                const auto P = d + "/" + desk + "-portals.conf";
                if (fs::is_regular_file(P, ec)) {
                    winner = P;
                    break;
                }
            }
            if (!winner.empty())
                break;
            const auto P = d + "/portals.conf";
            if (fs::is_regular_file(P, ec)) {
                winner = P;
                break;
            }
        }

        const std::set<std::string> SYSDIRS = {"/usr/share/xdg-desktop-portal", prefix + "/share/xdg-desktop-portal"};
        bool                        standard = false;
        if (winner.empty()) {
            raise(it.status, eStatus::WARN);
            it.details.push_back("no *-portals.conf found (xdg-desktop-portal falls back to all installed backends)");
        } else {
            const auto DIR  = fs::path{winner}.parent_path().string();
            const auto NAME = fs::path{winner}.filename().string();
            standard        = SYSDIRS.contains(DIR) && NAME == "abyssland-portals.conf";
            it.details.push_back(std::format("winning file: {}", winner));
            std::string prefs;
            for (const auto& l : split(readFile(winner), '\n'))
                if (!l.empty() && l[0] != '#' && l[0] != '[')
                    prefs += (prefs.empty() ? "" : ", ") + l;
            if (!prefs.empty())
                it.details.push_back(std::format("content: {}", prefs));
            if (!standard)
                raise(it.status, eStatus::MODIFIED);
        }

        // searched candidates that exist (so the user sees what else is shadowed)
        std::string others;
        for (const auto& d : dirs) {
            std::vector<std::string> names = {"portals.conf"};
            for (const auto& desk : desktops)
                names.insert(names.begin(), desk + "-portals.conf");
            names.push_back("abyssland-portals.conf");
            for (const auto& n : names) {
                const auto P = d + "/" + n;
                if (fs::is_regular_file(P, ec) && P != winner && others.find(P) == std::string::npos)
                    others += (others.empty() ? "" : ", ") + P;
            }
        }
        if (!others.empty())
            it.details.push_back(std::format("other existing files (not winning): {}", others));

        // installed *.portal files
        std::set<std::string> portalDirs;
        for (const auto& d : dirs)
            portalDirs.insert(d + "/portals");
        std::set<std::string> seen;
        for (const auto& d : portalDirs) {
            if (!fs::is_directory(d, ec))
                continue;
            std::vector<fs::path> files;
            for (const auto& e : fs::directory_iterator(d, ec))
                if (e.path().extension() == ".portal")
                    files.push_back(e.path());
            std::ranges::sort(files);
            for (const auto& f : files) {
                if (seen.contains(f.filename().string()))
                    continue;
                seen.insert(f.filename().string());
                std::string useIn = "<no UseIn>";
                for (const auto& l : split(readFile(f), '\n'))
                    if (l.rfind("UseIn=", 0) == 0)
                        useIn = l.substr(6);
                it.details.push_back(std::format("Portal {}: UseIn={}  ({})", f.filename().string(), useIn, d));
            }
        }
        if (!seen.contains("abyss.portal")) {
            raise(it.status, eStatus::MODIFIED);
            it.details.push_back("abyss.portal is not installed (another portal backend in use)");
        }

        it.summary = it.status == eStatus::WARN ? "portal resolution looks off" : it.status == eStatus::MODIFIED ? "modified: " + (winner.empty() ? "-" : winner) : "default (abyss)";
        if (it.status == eStatus::WARN)
            it.summary = "warning: " + (winner.empty() ? std::string{"no portals.conf"} : "token 'Abyssland' missing in XDG_CURRENT_DESKTOP");
        return it;
    }

    //
    // env + helpers
    //
    SItem checkEnv(const std::string& prefix) {
        SItem it;
        it.key = "env";

        const std::set<std::string> SYSTEMSET = {"ABYSSLAND_INSTANCE_SIGNATURE", "ABYSSLAND_CMD"};
        std::vector<std::string>    overrides;
        for (char** e = environ; e && *e; ++e) {
            const std::string kv = *e;
            const auto        eq = kv.find('=');
            const auto        k  = kv.substr(0, eq);
            if (k.rfind("ABYSSAL_", 0) == 0 || k.rfind("ABYSSLAND_", 0) == 0 || k.rfind("ABYSSCURSOR_", 0) == 0 || k == "ABYSS_SOURCE_DIR" || k == "ABYSS_BUILD_DIR" || k == "ABYSS_PREFIX") {
                if (SYSTEMSET.contains(k)) {
                    it.details.push_back(std::format("{} (set by the compositor)", kv));
                    continue;
                }
                overrides.push_back(kv);
            }
        }
        std::ranges::sort(overrides);
        for (const auto& o : overrides) {
            it.details.push_back(std::format("Override: {}", o));
            raise(it.status, eStatus::MODIFIED);
        }
        if (overrides.empty())
            it.details.push_back("no ABYSSAL_*/ABYSSLAND_*/ABYSSCURSOR_* overrides set");

        const auto XCD = envStr("XDG_CURRENT_DESKTOP");
        it.details.push_back(std::format("XDG_CURRENT_DESKTOP = {}", XCD.empty() ? "<unset>" : XCD));
        if (XCD != "Abyssland" && !XCD.empty() && lower(XCD).find("abyssland") != std::string::npos)
            raise(it.status, eStatus::MODIFIED);
        const auto WD = envStr("WAYLAND_DISPLAY");
        it.details.push_back(std::format("WAYLAND_DISPLAY = {}", WD.empty() ? "<unset>" : WD));

        // helpers: first hit in PATH vs. the one in <prefix>/bin
        for (const char* h : {"abyss-dialog", "abyss-share-picker"}) {
            const std::string STD = prefix + "/bin/" + h;
            std::string       found;
            for (const auto& d : split(envStr("PATH"), ':')) {
                const auto P = (d.empty() ? "." : d) + "/" + h;
                if (access(P.c_str(), X_OK) == 0 && fs::is_regular_file(P)) {
                    found = P;
                    break;
                }
            }
            if (found.empty()) {
                if (access(STD.c_str(), X_OK) == 0) {
                    it.details.push_back(std::format("Helper {}: not in PATH, only {} exists (Abyssland looks it up via PATH)", h, STD));
                    raise(it.status, eStatus::WARN);
                } else {
                    it.details.push_back(std::format("Helper {}: not found (neither in PATH nor {})", h, STD));
                    raise(it.status, eStatus::WARN);
                }
            } else if (sameFile(found, STD)) {
                it.details.push_back(std::format("Helper {}: {} (default)", h, found));
            } else {
                it.details.push_back(std::format("Helper {}: {} (modified, default would be {})", h, found, STD));
                raise(it.status, eStatus::MODIFIED);
            }
        }

        it.summary = it.status == eStatus::OK ? "no overrides, helpers from " + prefix + "/bin" :
            it.status == eStatus::MODIFIED    ? "modified (user's responsibility)" :
                                                "warning: check helpers or environment";
        return it;
    }

    //
    // config
    //
    SItem checkConfig(const std::string& prefix, const std::string& sourceDir) {
        SItem it;
        it.key = "config";

        const auto  XCH  = envStr("XDG_CONFIG_HOME");
        const auto  CDIR = (XCH.empty() ? homeDir() + "/.config" : XCH) + "/abyss";
        const auto  CFG  = fs::path{CDIR} / "config.hpp";
        std::error_code ec;

        if (fs::is_regular_file(CFG, ec)) {
            it.details.push_back(std::format("{}: present", CFG.string()));
            const auto DEF = fs::path{sourceDir} / "config.def.hpp";
            if (fs::is_regular_file(DEF, ec)) {
                if (readFile(DEF) == readFile(CFG)) {
                    it.details.push_back("identical to config.def.hpp (default)");
                } else {
                    it.details.push_back("differs from config.def.hpp (custom config)");
                    raise(it.status, eStatus::MODIFIED);
                }
            } else {
                it.details.push_back("config.def.hpp not found, no comparison");
                raise(it.status, eStatus::MODIFIED);
            }
        } else {
            it.details.push_back(std::format("{}: not present (default config.def.hpp)", CFG.string()));
        }

        const auto LG = fs::path{prefix} / "bin" / "Abyssland.last-good";
        if (fs::is_regular_file(LG, ec)) {
            it.details.push_back(std::format("{}: present ({} bytes), fallback for start-abyssland", LG.string(), fs::file_size(LG, ec)));
        } else {
            it.details.push_back(std::format("{}: missing (no 'abyssctl reload' installed yet, no fallback binary)", LG.string()));
            raise(it.status, eStatus::WARN);
        }

        it.summary = it.status == eStatus::OK ? "default config, last-good present" : it.status == eStatus::MODIFIED ? "modified: custom config.hpp" : "warning: last-good missing";
        return it;
    }

    //
    // protocols
    //
    SItem checkProtocols(const std::optional<std::string>& protocolsJson) {
        SItem it;
        it.key = "protocols";

        if (!protocolsJson) {
            it.status  = eStatus::INFO;
            it.summary = "instance not reachable, not checked";
            return it;
        }

        const std::regex RE("\"name\":\"([^\"]*)\"[^}]*?\"state\":\"([^\"]*)\"");
        size_t           total = 0;
        std::vector<std::string> off, notCreated;
        for (auto m = std::sregex_iterator(protocolsJson->begin(), protocolsJson->end(), RE); m != std::sregex_iterator(); ++m) {
            ++total;
            if ((*m)[2] == "off")
                off.push_back((*m)[1]);
            else if ((*m)[2] == "not-created")
                notCreated.push_back((*m)[1]);
        }

        it.details.push_back(std::format("{} known protocols, {} disabled", total, off.size()));
        for (const auto& n : off)
            it.details.push_back(std::format("off: {}", n));
        if (!notCreated.empty())
            it.details.push_back(std::format("{} enabled, but without a global in this session (hardware dependent)", notCreated.size()));

        if (total == 0) {
            it.status  = eStatus::WARN;
            it.summary = "could not parse the instance reply";
        } else if (!off.empty()) {
            it.status  = eStatus::MODIFIED;
            it.summary = std::format("modified: {} protocol(s) disabled", off.size());
        } else {
            it.summary = "all protocols on (default)";
        }
        return it;
    }

}

int runDoctor(bool json, const std::string& prefix, const std::string& sourceDir, const std::string& instanceSig,
              const std::function<std::optional<std::string>(std::string_view)>& query) {
    std::optional<std::string> versionJson, protocolsJson;
    if (!instanceSig.empty() && query) {
        versionJson = query("j/version");
        if (versionJson && versionJson->find("\"commit\"") == std::string::npos)
            versionJson.reset();
        protocolsJson = query("j/protocols");
        if (protocolsJson && protocolsJson->rfind("[", 0) != 0 && protocolsJson->find("[") == std::string::npos)
            protocolsJson.reset();
    }

    std::vector<SItem> items;
    items.push_back(checkVersion(sourceDir, versionJson ? instanceSig : "", versionJson));
    items.push_back(checkPortals(prefix));
    items.push_back(checkEnv(prefix));
    items.push_back(checkConfig(prefix, sourceDir));
    items.push_back(checkProtocols(protocolsJson));

    bool anyModified = false, anyWarn = false;
    for (const auto& i : items) {
        anyModified |= i.status == eStatus::MODIFIED;
        anyWarn |= i.status == eStatus::WARN;
    }

    if (json) {
        std::string out = "{\n  \"instance\": \"" + jsonEscape(instanceSig) + "\",\n  \"reachable\": " + (versionJson ? "true" : "false") + ",\n  \"modified\": " +
            (anyModified ? "true" : "false") + ",\n  \"warnings\": " + (anyWarn ? "true" : "false") + ",\n  \"checks\": [";
        for (size_t n = 0; n < items.size(); ++n) {
            const auto& i = items[n];
            out += std::format("{}\n    {{\"name\": \"{}\", \"status\": \"{}\", \"summary\": \"{}\", \"details\": [", n ? "," : "", i.key, statusKey(i.status), jsonEscape(i.summary));
            for (size_t d = 0; d < i.details.size(); ++d)
                out += std::format("{}\"{}\"", d ? ", " : "", jsonEscape(i.details[d]));
            out += "]}";
        }
        out += "\n  ]\n}";
        std::println("{}", out);
    } else {
        std::println("abyssctl doctor (read-only; a deviation from the default is not an error)\n");
        for (const auto& i : items) {
            std::println("[{:<9}] {}: {}", statusLabel(i.status), i.key, i.summary);
            for (const auto& d : i.details)
                std::println("             {}", d);
        }
        std::println("\nResult: {}", anyWarn ? "warnings present" : anyModified ? "modified (user's responsibility)" : "default");
    }

    return 0;
}
