#include <re2/re2.h>

#include <cctype>
#include <cerrno>
#include <netdb.h>
#include <netinet/in.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <pwd.h>
#include <unistd.h>
#include <algorithm>
#include <csignal>
#include <ranges>
#include <optional>
#include <charconv>

#include <iostream>
#include <string>
#include <print>
#include <fstream>
#include <vector>
#include <filesystem>
#include <cstdarg>
#include <thread>
#include <regex>
#include <sys/wait.h>
#include <abyssutils/string/String.hpp>
#include <abyssutils/memory/Casts.hpp>
using namespace Abyssutils::String;
using namespace Abyssutils::Memory;

#include "Strings.hpp"
#include "abysspaper/Abysspaper.hpp"
#include "Doctor.hpp"

#include <readline/readline.h>
#include <readline/history.h>

std::string instanceSignature;
bool        quiet = false;

struct SInstanceData {
    std::string id;
    uint64_t    time;
    uint64_t    pid;
    std::string wlSocket;
};

void log(const std::string_view str) {
    if (quiet)
        return;

    std::println("{}", str);
}

static bool writeAll(const int fd, std::string_view data) {
    size_t totalWritten = 0;
    while (totalWritten < data.size()) {
        const auto written = write(fd, data.data() + totalWritten, data.size() - totalWritten);
        if (written > 0) {
            totalWritten += sc<size_t>(written);
            continue;
        }

        if (written < 0 && errno == EINTR)
            continue;

        return false;
    }

    return true;
}

static int getUID() {
    const auto UID   = getuid();
    const auto PWUID = getpwuid(UID);
    return PWUID ? PWUID->pw_uid : UID;
}

std::string getRuntimeDir() {
    const auto XDG = getenv("XDG_RUNTIME_DIR");

    if (!XDG) {
        const std::string USERID = std::to_string(getUID());
        return "/run/user/" + USERID + "/abyss";
    }

    return std::string{XDG} + "/abyss";
}

static std::optional<uint64_t> toUInt64(const std::string_view str) {
    uint64_t value       = 0;
    const auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
    if (ec != std::errc() || ptr != str.data() + str.size())
        return std::nullopt;
    return value;
}

static std::optional<SInstanceData> parseInstance(const std::filesystem::directory_entry& entry) {
    if (!entry.is_directory())
        return std::nullopt;

    const auto    lockPath = entry.path() / "abyssland.lock";
    std::ifstream ifs(lockPath);
    if (!ifs.is_open())
        return std::nullopt;

    SInstanceData data;
    data.id = entry.path().filename().string();

    const auto first = std::string_view{data.id}.find_first_of('_');
    const auto last  = std::string_view{data.id}.find_last_of('_');
    if (first == std::string_view::npos || last == std::string_view::npos || last <= first)
        return std::nullopt;

    auto time = toUInt64(std::string_view{data.id}.substr(first + 1, last - first - 1));
    if (!time)
        return std::nullopt;
    data.time = *time;

    std::string line;
    if (!std::getline(ifs, line))
        return std::nullopt;

    auto pid = toUInt64(std::string_view{line});
    if (!pid)
        return std::nullopt;
    data.pid = *pid;

    if (!std::getline(ifs, data.wlSocket))
        return std::nullopt;

    if (std::getline(ifs, line) && !line.empty())
        return std::nullopt; // more lines than expected

    return data;
}

std::vector<SInstanceData> instances() {
    std::vector<SInstanceData> result;

    std::error_code            ec;
    const auto                 runtimeDir = getRuntimeDir();
    if (!std::filesystem::exists(runtimeDir, ec) || ec)
        return result;

    std::filesystem::directory_iterator it(runtimeDir, std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec)
        return result;

    for (const auto& el : it) {
        if (auto instance = parseInstance(el))
            result.emplace_back(std::move(*instance));
    }

    std::erase_if(result, [](const auto& el) { return kill(el.pid, 0) != 0 && errno == ESRCH; });

    std::ranges::sort(result, {}, &SInstanceData::time);

    return result;
}

static volatile bool sigintReceived = false;
void                 intHandler(int sig) {
    sigintReceived = true;
    std::println("[abyssctl] SIGINT received, closing connection");
}

int rollingRead(const int socket) {
    sigintReceived = false;
    signal(SIGINT, intHandler);

    constexpr size_t              BUFFER_SIZE = 8192;
    std::array<char, BUFFER_SIZE> buffer      = {0};
    long                          sizeWritten = 0;
    std::println("[abyssctl] reading from socket following up log:");
    while (!sigintReceived) {
        sizeWritten = read(socket, buffer.data(), BUFFER_SIZE);
        if (sizeWritten < 0 && errno != EAGAIN) {
            if (errno != EINTR)
                std::println("Couldn't read (5): {}: {}", strerror(errno), errno);
            close(socket);
            return 5;
        }

        if (sizeWritten == 0)
            break;

        if (sizeWritten > 0) {
            std::println("{}", std::string(buffer.data(), sizeWritten));
            buffer.fill('\0');
        }

        usleep(100000);
    }
    close(socket);
    return 0;
}

int request(std::string_view arg, int minArgs = 0, bool needRoll = false) {
    const auto SERVERSOCKET = socket(AF_UNIX, SOCK_STREAM, 0);

    if (SERVERSOCKET < 0) {
        log("Couldn't open a socket (1)");
        return 1;
    }

    auto t = timeval{.tv_sec = 5, .tv_usec = 0};
    if (setsockopt(SERVERSOCKET, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(struct timeval)) < 0) {
        log("Couldn't set socket timeout (2)");
        return 2;
    }

    const auto ARGS = std::count(arg.begin(), arg.end(), ' ');

    if (ARGS < minArgs) {
        log(std::format("Not enough arguments in '{}', expected at least {}", arg, minArgs));
        return -1;
    }

    if (instanceSignature.empty()) {
        log("ABYSSLAND_INSTANCE_SIGNATURE was not set! (Is Abyssland running?) (3)");
        return 3;
    }

    sockaddr_un serverAddress = {0};
    serverAddress.sun_family  = AF_UNIX;

    std::string socketPath = getRuntimeDir() + "/" + instanceSignature + "/.socket.sock";

    strncpy(serverAddress.sun_path, socketPath.c_str(), sizeof(serverAddress.sun_path) - 1);

    if (connect(SERVERSOCKET, rc<sockaddr*>(&serverAddress), SUN_LEN(&serverAddress)) < 0) {
        log("Couldn't connect to " + socketPath + ". (4)");
        return 4;
    }

    if (!writeAll(SERVERSOCKET, arg)) {
        log("Couldn't write (5)");
        return 5;
    }

    if (needRoll)
        return rollingRead(SERVERSOCKET);

    std::string      reply               = "";
    constexpr size_t BUFFER_SIZE         = 8192;
    char             buffer[BUFFER_SIZE] = {0};

    // read all data until server closes the connection
    // this handles partial writes on the server side under high load
    while (true) {
        const auto sizeRead = read(SERVERSOCKET, buffer, BUFFER_SIZE);

        if (sizeRead < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EWOULDBLOCK)
                log("Abyssland IPC didn't respond in time\n");
            log("Couldn't read (6)");
            return 6;
        }

        if (sizeRead == 0) {
            // server closed connection, we're done
            break;
        }

        reply.append(buffer, sc<size_t>(sizeRead));
    }

    close(SERVERSOCKET);

    log(reply);

    if (reply.starts_with("error:"))
        return 7;

    return 0;
}

// like request(), but returns the reply instead of printing it (used by `doctor`, read-only requests only)
static std::optional<std::string> requestCapture(std::string_view arg) {
    if (instanceSignature.empty())
        return std::nullopt;

    const auto FD = socket(AF_UNIX, SOCK_STREAM, 0);
    if (FD < 0)
        return std::nullopt;

    auto t = timeval{.tv_sec = 5, .tv_usec = 0};
    setsockopt(FD, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t));

    sockaddr_un addr = {0};
    addr.sun_family  = AF_UNIX;
    const std::string PATH = getRuntimeDir() + "/" + instanceSignature + "/.socket.sock";
    strncpy(addr.sun_path, PATH.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(FD, rc<sockaddr*>(&addr), SUN_LEN(&addr)) < 0 || !writeAll(FD, arg)) {
        close(FD);
        return std::nullopt;
    }

    std::string reply;
    char        buf[8192];
    while (true) {
        const auto N = read(FD, buf, sizeof(buf));
        if (N < 0 && errno == EINTR)
            continue;
        if (N <= 0)
            break;
        reply.append(buf, sc<size_t>(N));
    }
    close(FD);

    if (reply.empty() || reply.starts_with("error:"))
        return std::nullopt;
    return reply;
}

int requestIPC(std::string_view filename, std::string_view arg) {
    const auto SERVERSOCKET = socket(AF_UNIX, SOCK_STREAM, 0);

    if (SERVERSOCKET < 0) {
        log("Couldn't open a socket (1)");
        return 1;
    }

    if (instanceSignature.empty()) {
        log("ABYSSLAND_INSTANCE_SIGNATURE was not set! (Is Abyssland running?)");
        return 2;
    }

    sockaddr_un serverAddress = {0};
    serverAddress.sun_family  = AF_UNIX;

    std::string socketPath = getRuntimeDir() + "/" + instanceSignature + "/" + std::string{filename};

    strncpy(serverAddress.sun_path, socketPath.c_str(), sizeof(serverAddress.sun_path) - 1);

    if (connect(SERVERSOCKET, rc<sockaddr*>(&serverAddress), SUN_LEN(&serverAddress)) < 0) {
        log("Couldn't connect to " + socketPath + ". (3)");
        return 3;
    }

    arg = arg.substr(arg.find_first_of('/') + 1); // strip flags
    arg = arg.substr(arg.find_first_of(' ') + 1); // strip "abysspaper"

    if (!writeAll(SERVERSOCKET, arg)) {
        log("Couldn't write (4)");
        return 4;
    }
    constexpr size_t BUFFER_SIZE         = 8192;
    char             buffer[BUFFER_SIZE] = {0};

    ssize_t          sizeRead = 0;
    do {
        sizeRead = read(SERVERSOCKET, buffer, BUFFER_SIZE);
    } while (sizeRead < 0 && errno == EINTR);

    if (sizeRead < 0) {
        log("Couldn't read (5)");
        return 5;
    }

    close(SERVERSOCKET);

    log(std::string(buffer, sc<size_t>(sizeRead)));

    return 0;
}

int requestAbysssunset(std::string_view arg) {
    return requestIPC(".abysssunset.sock", arg);
}

void batchRequest(std::string_view arg, bool json) {
    std::string commands(arg.substr(arg.find_first_of(' ') + 1));

    if (json) {
        RE2::GlobalReplace(&commands, ";\\s*", ";j/");
        commands.insert(0, "j/");
    }

    std::string rq = "[[BATCH]]" + commands;
    request(rq);
}

void instancesRequest(bool json) {
    std::string result = "";

    // gather instance data
    std::vector<SInstanceData> inst = instances();

    if (!json) {
        for (auto const& el : inst) {
            result += std::format("instance {}:\n\ttime: {}\n\tpid: {}\n\twl socket: {}\n\n", el.id, el.time, el.pid, el.wlSocket);
        }
    } else {
        result += '[';
        for (auto const& el : inst) {
            result += std::format(R"#(
{{
    "instance": "{}",
    "time": {},
    "pid": {},
    "wl_socket": "{}"
}},)#",
                                  el.id, el.time, el.pid, el.wlSocket);
        }

        result.pop_back();
        result += "\n]";
    }

    log(result + "\n");
}


// ---- reload: rebuild + install + restart ------------------------------------------------------

#ifndef ABYSS_SOURCE_DIR
#define ABYSS_SOURCE_DIR ""
#endif
#ifndef ABYSS_BUILD_DIR
#define ABYSS_BUILD_DIR ""
#endif
#ifndef ABYSS_DEFAULT_PREFIX
#define ABYSS_DEFAULT_PREFIX "/usr"
#endif

static std::string envOr(const char* name, const char* fallback) {
    const auto E = getenv(name);
    return (E && *E) ? E : fallback;
}

struct SRunResult {
    int         exitCode = -1;
    std::string output;
};

// runs a command, merging stdout+stderr; every line is passed to onLine as it arrives
template <typename F>
static SRunResult runCapture(const std::vector<std::string>& argv, F&& onLine) {
    SRunResult res;
    int        fds[2];
    if (pipe(fds) != 0) {
        res.output = "pipe() failed";
        return res;
    }

    const auto PID = fork();
    if (PID < 0) {
        close(fds[0]);
        close(fds[1]);
        res.output = "fork() failed";
        return res;
    }

    if (PID == 0) {
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        close(fds[0]);
        close(fds[1]);
        std::vector<char*> cargv;
        for (const auto& a : argv)
            cargv.push_back(const_cast<char*>(a.c_str()));
        cargv.push_back(nullptr);
        execvp(cargv[0], cargv.data());
        std::println(stderr, "failed to execute {}: {}", argv[0], strerror(errno));
        _exit(127);
    }

    close(fds[1]);

    std::string line;
    char        buf[4096];
    while (true) {
        const auto N = read(fds[0], buf, sizeof(buf));
        if (N < 0 && errno == EINTR)
            continue;
        if (N <= 0)
            break;
        for (ssize_t i = 0; i < N; ++i) {
            if (buf[i] == '\n') {
                res.output += line + "\n";
                onLine(line);
                line.clear();
            } else
                line += buf[i];
        }
    }
    if (!line.empty()) {
        res.output += line + "\n";
        onLine(line);
    }
    close(fds[0]);

    int status = 0;
    while (waitpid(PID, &status, 0) < 0 && errno == EINTR)
        ;
    res.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    return res;
}

static std::string firstBuildError(const std::string& output, const std::string& sourceDir) {
    static const std::regex COMPILER_ERR(R"(^(.+?:\d+(:\d+)?:\s*(fatal )?error:.*)$)");
    static const std::regex OTHER_ERR(R"(^(CMake Error.*|.*undefined reference to.*|.*ld(\.lld)?: error.*|.*\bld: .*|.*Error \d+.*|ninja: error.*)$)");

    std::string firstOther, lastLine;
    size_t      pos = 0;
    while (pos < output.size()) {
        auto end = output.find('\n', pos);
        if (end == std::string::npos)
            end = output.size();
        std::string l = output.substr(pos, end - pos);
        pos           = end + 1;
        if (l.empty())
            continue;
        lastLine = l;
        std::smatch m;
        if (std::regex_match(l, m, COMPILER_ERR)) {
            std::string r = m[1];
            if (!sourceDir.empty() && r.starts_with(sourceDir + "/"))
                r.erase(0, sourceDir.size() + 1);
            return r;
        }
        if (firstOther.empty() && std::regex_match(l, OTHER_ERR))
            firstOther = l;
    }
    return !firstOther.empty() ? firstOther : lastLine;
}

static void sendErrorNotification(const std::string& text) {
    if (instanceSignature.empty())
        return;

    std::string msg = text.substr(0, 400);
    for (auto& c : msg)
        if (c == '\n' || c == '\r' || c == ';')
            c = ' ';
    const bool wasQuiet = quiet;
    quiet               = true;
    request("/notify 3 8000 rgb(ff5555) Abyssland reload failed: " + msg);
    quiet = wasQuiet;
}

static int reloadRebuild(bool dryRun) {
    const std::string SRC    = envOr("ABYSS_SOURCE_DIR", ABYSS_SOURCE_DIR);
    const std::string BUILD  = envOr("ABYSS_BUILD_DIR", ABYSS_BUILD_DIR);
    const std::string PREFIX = envOr("ABYSS_PREFIX", ABYSS_DEFAULT_PREFIX);

    std::error_code ec;
    if (BUILD.empty() || !std::filesystem::exists(std::filesystem::path{BUILD} / "CMakeCache.txt", ec)) {
        const std::string MSG = "no build directory found (" + (BUILD.empty() ? std::string{"<unset>"} : BUILD) + "), set ABYSS_BUILD_DIR";
        std::println(stderr, "abyssctl: {}", MSG);
        sendErrorNotification(MSG);
        return 1;
    }

    const unsigned    JOBS = std::max(1u, std::thread::hardware_concurrency() / 2);

    std::println("abyssctl: building in {} (-j{})", BUILD, JOBS);
    std::fflush(stdout);

    static const std::regex PROGRESS(R"(^\[\s*\d+/\d+\].*)");
    auto                    BUILDRES = runCapture({"cmake", "--build", BUILD, "--target", "Abyssland", "-j" + std::to_string(JOBS)}, [](const std::string& l) {
        if (std::regex_match(l, PROGRESS) || l.starts_with("ninja:") || l.starts_with("make")) {
            std::println("{}", l);
            std::fflush(stdout);
        }
    });

    if (BUILDRES.exitCode != 0) {
        const auto LOGPATH = std::filesystem::path{BUILD} / "abyssctl-reload.log";
        std::ofstream{LOGPATH} << BUILDRES.output;
        std::string err = firstBuildError(BUILDRES.output, SRC);
        if (err.empty())
            err = "build failed (exit " + std::to_string(BUILDRES.exitCode) + ")";
        std::println(stderr, "abyssctl: build failed:\n{}\n(full log: {})", err, LOGPATH.string());
        sendErrorNotification(err);
        return 1;
    }

    if (dryRun) {
        std::println("abyssctl: build ok (dry run, nothing installed)");
        return 0;
    }

    // keep the currently installed binary as fallback for start-abyssland
    const auto BINDIR   = std::filesystem::path{PREFIX} / "bin";
    const auto BIN      = BINDIR / "Abyssland";
    const auto LASTGOOD = BINDIR / "Abyssland.last-good";
    if (std::filesystem::is_regular_file(BIN, ec)) {
        const auto TMP = BINDIR / "Abyssland.last-good.tmp";
        std::filesystem::copy_file(BIN, TMP, std::filesystem::copy_options::overwrite_existing, ec);
        if (!ec)
            std::filesystem::rename(TMP, LASTGOOD, ec);
        if (ec) {
            const std::string MSG = "could not back up " + BIN.string() + " to last-good: " + ec.message();
            std::println(stderr, "abyssctl: {}", MSG);
            sendErrorNotification(MSG);
            return 1;
        }
    }

    auto INSTALLRES = runCapture({"cmake", "--install", BUILD, "--prefix", PREFIX}, [](const std::string&) {});
    if (INSTALLRES.exitCode != 0) {
        std::string err = firstBuildError(INSTALLRES.output, SRC);
        std::println(stderr, "abyssctl: install failed:\n{}", err);
        sendErrorNotification("install failed: " + err);
        return 1;
    }

    std::println("abyssctl: installed to {}", PREFIX);

    if (instanceSignature.empty()) {
        std::println("abyssctl: no running Abyssland instance, not restarting");
        return 0;
    }

    const int RET = request("/restart");
    if (RET != 0)
        std::println(stderr, "abyssctl: the new binary is installed, but Abyssland was not restarted");
    return RET;
}

std::vector<std::string> splitArgs(int argc, char** argv) {
    std::vector<std::string> result;

    for (auto i = 1 /* skip the executable */; i < argc; ++i)
        result.emplace_back(argv[i]);

    return result;
}

int main(int argc, char** argv) {
    bool parseArgs = true;

    if (argc < 2) {
        std::println("{}", USAGE);
        return 1;
    }

    std::string fullRequest      = "";
    std::string fullArgs         = "";
    const auto  ARGS             = splitArgs(argc, argv);
    bool        json             = false;
    bool        needRoll         = false;
    bool        noBuild          = false;
    bool        dryRun           = false;
    std::string overrideInstance = "";

    for (std::size_t i = 0; i < ARGS.size(); ++i) {
        if (ARGS[i] == "--") {
            // Stop parsing arguments after --
            parseArgs = false;
            continue;
        }
        if (parseArgs && (ARGS[i][0] == '-') && !(isNumber(ARGS[i], true) || isNumber(ARGS[i].substr(0, ARGS[i].length() - 1), true)) /* For stuff like -2 or -2, */) {
            // parse
            if (ARGS[i] == "-j" && !fullArgs.contains("j")) {
                fullArgs += "j";
                json = true;
            } else if (ARGS[i] == "-r" && !fullArgs.contains("r")) {
                fullArgs += "r";
            } else if (ARGS[i] == "-a" && !fullArgs.contains("a")) {
                fullArgs += "a";
            } else if ((ARGS[i] == "-c" || ARGS[i] == "--config") && !fullArgs.contains("c")) {
                fullArgs += "c";
            } else if ((ARGS[i] == "-f" || ARGS[i] == "--follow") && !fullArgs.contains("f")) {
                fullArgs += "f";
                needRoll = true;
            } else if (ARGS[i] == "--no-build" && fullRequest.starts_with("reload")) {
                noBuild = true;
            } else if (ARGS[i] == "--dry-run" && fullRequest.starts_with("reload")) {
                dryRun = true;
            } else if (ARGS[i] == "--batch") {
                fullRequest = "--batch ";
            } else if (ARGS[i] == "--instance" || ARGS[i] == "-i") {
                ++i;

                if (i >= ARGS.size()) {
                    std::println("{}", USAGE);
                    return 1;
                }

                overrideInstance = ARGS[i];
            } else if (ARGS[i] == "-q" || ARGS[i] == "--quiet") {
                quiet = true;
            } else if (ARGS[i] == "--help") {
                const std::string& cmd = ARGS[0];

                if (cmd == "abysspaper") {
                    std::println("{}", ABYSSPAPER_HELP);
                } else if (cmd == "abysssunset") {
                    std::println("{}", ABYSSSUNSET_HELP);
                } else if (cmd == "reload") {
                    std::println("{}", RELOAD_HELP);
                } else if (cmd == "notify") {
                    std::println("{}", NOTIFY_HELP);
                } else if (cmd == "output") {
                    std::println("{}", OUTPUT_HELP);
                } else if (cmd == "setprop") {
                    std::println("{}", SETPROP_HELP);
                } else if (cmd == "getprop") {
                    std::println("{}", GETPROP_HELP);
                } else if (cmd == "switchxkblayout") {
                    std::println("{}", SWITCHXKBLAYOUT_HELP);
                } else {
                    std::println("{}", USAGE);
                }

                return 1;
            } else {
                std::println("{}", USAGE);
                return 1;
            }

            continue;
        }

        fullRequest += ARGS[i] + " ";
    }

    if (fullRequest.empty()) {
        std::println("{}", USAGE);
        return 1;
    }

    fullRequest.pop_back(); // remove trailing space

    fullRequest = fullArgs + "/" + fullRequest;

    // `run <name> [args]` is passed to the compositor verbatim; its arguments must not be mistaken for other commands
    const bool ISRUN = fullRequest.substr(fullRequest.find('/') + 1) == "run" || fullRequest.substr(fullRequest.find('/') + 1).starts_with("run ");

    // instances is HIS-independent
    if (!ISRUN && fullRequest.contains("/instances")) {
        instancesRequest(json);
        return 0;
    }

    const bool PLAINRELOAD = fullRequest.ends_with("/reload") && !noBuild;

    if ((noBuild || dryRun) && !fullRequest.ends_with("/reload")) {
        log("--no-build and --dry-run only apply to a plain 'reload'");
        return 1;
    }

    if (noBuild && dryRun) {
        log("--no-build and --dry-run can't be combined");
        return 1;
    }

    // a dry run needs no running instance
    if (PLAINRELOAD && dryRun)
        return reloadRebuild(true);

    if (needRoll && !fullRequest.contains("/rollinglog")) {
        log("only 'rollinglog' command supports '--follow' option");
        return 1;
    }

    // doctor: read-only, works without a running instance (instance part is skipped then)
    if (!ISRUN && fullRequest.ends_with("/doctor")) {
        if (overrideInstance.contains("_"))
            instanceSignature = overrideInstance;
        else if (!overrideInstance.empty() && isNumber(overrideInstance, false)) {
            const auto INSTANCENO = std::stoi(overrideInstance);
            const auto INSTS      = instances();
            if (INSTANCENO >= 0 && sc<std::size_t>(INSTANCENO) < INSTS.size())
                instanceSignature = INSTS[INSTANCENO].id;
        } else if (const auto ISIG = getenv("ABYSSLAND_INSTANCE_SIGNATURE"); ISIG && *ISIG)
            instanceSignature = ISIG;
        else if (const auto INSTS = instances(); !INSTS.empty())
            instanceSignature = INSTS.back().id;

        // the compile-time prefix is only right for installed builds; a dev build tree defaults to /usr/local
        std::string prefix = envOr("ABYSS_PREFIX", ABYSS_DEFAULT_PREFIX);
        if (!getenv("ABYSS_PREFIX") && !std::filesystem::exists(std::filesystem::path{prefix} / "bin" / "Abyssland") && std::filesystem::exists("/opt/abyss/bin/Abyssland"))
            prefix = "/opt/abyss";

        return runDoctor(json, prefix, envOr("ABYSS_SOURCE_DIR", ABYSS_SOURCE_DIR), instanceSignature, requestCapture);
    }

    if (overrideInstance.contains("_"))
        instanceSignature = overrideInstance;
    else if (!overrideInstance.empty()) {
        if (!isNumber(overrideInstance, false)) {
            log("instance invalid\n");
            return 1;
        }

        const auto INSTANCENO = std::stoi(overrideInstance);

        const auto INSTANCES = instances();

        if (INSTANCENO < 0 || sc<std::size_t>(INSTANCENO) >= INSTANCES.size()) {
            log("no such instance\n");
            return 1;
        }

        instanceSignature = INSTANCES[INSTANCENO].id;
    } else {
        const auto ISIG = getenv("ABYSSLAND_INSTANCE_SIGNATURE");

        if (!ISIG) {
            if (PLAINRELOAD)
                return reloadRebuild(false);
            log("ABYSSLAND_INSTANCE_SIGNATURE not set! (is abyssland running?)\n");
            return 1;
        }

        instanceSignature = ISIG;
    }

    int exitStatus = 0;

    if (PLAINRELOAD)
        return reloadRebuild(false);

    if (ISRUN)
        exitStatus = request(fullRequest);
    else if (fullRequest.contains("/--batch"))
        batchRequest(fullRequest, json);
    else if (fullRequest.contains("/abysspaper")) {
        auto result = Abysspaper::makeAbysspaperRequest(fullRequest);
        if (!result)
            log(std::format("error: {}", result.error()));
        exitStatus = !result;
    } else if (fullRequest.contains("/abysssunset"))
        exitStatus = requestAbysssunset(fullRequest);
    else if (fullRequest.contains("/switchxkblayout"))
        exitStatus = request(fullRequest, 2);
    else if (fullRequest.contains("/seterror"))
        exitStatus = request(fullRequest, 1);
    else if (fullRequest.contains("/setprop"))
        exitStatus = request(fullRequest, 3);
    else if (fullRequest.contains("/dismissnotify"))
        exitStatus = request(fullRequest, 0);
    else if (fullRequest.contains("/notify"))
        exitStatus = request(fullRequest, 2);
    else if (fullRequest.contains("/output"))
        exitStatus = request(fullRequest, 2);
    else if (fullRequest.contains("/setcursor"))
        exitStatus = request(fullRequest, 1);
    else if (fullRequest.contains("/dispatch"))
        exitStatus = request(fullRequest, 1);
    else if (fullRequest.contains("/decorations"))
        exitStatus = request(fullRequest, 1);
    else if (fullRequest.contains("/--help"))
        std::println("{}", USAGE);
    else if (fullRequest.contains("/rollinglog") && needRoll)
        exitStatus = request(fullRequest, 0, true);
    else {
        exitStatus = request(fullRequest);
    }

    std::cout << std::flush;
    return exitStatus;
}
