#include "Saver.hpp"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <format>
#include <cairo/cairo.h>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/wait.h>

#include "../Compositor.hpp"
#include "../debug/log/Logger.hpp"
#include "../event/EventBus.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../output/Monitor.hpp"
#include "../render/Renderer.hpp"
#include "../render/pass/RectPassElement.hpp"
#include "../render/pass/TexPassElement.hpp"
#include "../state/MonitorState.hpp"

extern char** environ;

// pidfd helpers: signalling by pid could hit an unrelated process after the pid was reused
static int pidfdOpen(pid_t pid) {
    return sc<int>(syscall(SYS_pidfd_open, pid, 0));
}
static bool pidfdSignal(int pidfd, pid_t pid, int sig) {
    if (pidfd >= 0)
        return syscall(SYS_pidfd_send_signal, pidfd, sig, nullptr, 0) == 0;
    return false; // never fall back to the bare pid
}
static bool pidfdExited(int pidfd) {
    pollfd p = {.fd = pidfd, .events = POLLIN, .revents = 0};
    return pidfd < 0 || poll(&p, 1, 0) != 0;
}

namespace abyss::detail {
    void clearCanvasImages();
}

using namespace Saver;

UP<CSaverManager>& Saver::mgr() {
    static UP<CSaverManager> p = makeUnique<CSaverManager>();
    return p;
}

CSaverManager::CSaverManager() {
    // a removed monitor drops its texture (a new one is created lazily when it comes back)
    m_monRemoved = Event::bus()->m_events.monitor.removed.listen([this](PHLMONITOR m) {
        if (m)
            m_mons.erase(m->m_id);
    });
    m_exit = Event::bus()->m_events.exit.listen([this] {
        m_exiting = true; // no event loop left to run the SIGKILL fallback timer
        reset();
    });
}

CSaverManager::~CSaverManager() {
    stopTimer();
    stopCava();
}

void CSaverManager::registerDef(SP<SDef> def) {
    if (def)
        m_defs.emplace_back(std::move(def));
}

void CSaverManager::reset() {
    hide();
    m_defs.clear();
}

size_t CSaverManager::defCount() const {
    return m_defs.size();
}

bool CSaverManager::active() const {
    return !!m_active;
}

bool CSaverManager::showFirst() {
    if (m_defs.empty())
        return false;
    show(m_defs.front());
    return true;
}

void CSaverManager::show(SP<SDef> def) {
    if (!def)
        return;
    if (m_active == def)
        return;

    const bool WASACTIVE = !!m_active;
    if (WASACTIVE) {
        // replaced by another one: restart cleanly (cava / timer settings differ)
        stopTimer();
        stopCava();
        m_mons.clear();
    }

    m_active = std::move(def);
    m_start  = std::chrono::steady_clock::now();
    m_tick++;
    m_swallowedKeys.clear();
    m_swallowedButtons.clear();
    m_audio.assign(std::clamp(m_active->opts.bars, 1, 512), 0.F);

    if (m_active->opts.audio)
        startCava();
    startTimer();

    if (g_pAbyssRenderer) {
        g_pAbyssRenderer->ensureCursorRenderingMode();
        invalidateAll();
    }
}

void CSaverManager::hide() {
    if (!m_active)
        return;

    m_active.reset();
    abyss::detail::clearCanvasImages();
    stopTimer();
    stopCava();
    m_mons.clear();          // textures are freed with the states

    if (g_pAbyssRenderer && !g_pCompositor->m_isShuttingDown) {
        g_pAbyssRenderer->ensureCursorRenderingMode();
        for (auto const& m : State::monitorState()->monitors()) {
            g_pAbyssRenderer->damageMonitor(m);
            m->scheduleFrame();
        }
    }
}

void CSaverManager::invalidateAll() {
    for (auto const& m : State::monitorState()->monitors()) {
        g_pAbyssRenderer->damageMonitor(m);
        m->scheduleFrame();
    }
}

// ---- timer ----

void CSaverManager::startTimer() {
    const int FPS = std::clamp(m_active->opts.fps, 0, 144);
    if (FPS == 0)
        return; // drawn once
    const auto INTERVAL = std::chrono::microseconds(1000000 / FPS);

    m_timer = makeShared<CEventLoopTimer>(
        INTERVAL,
        [this, INTERVAL](SP<CEventLoopTimer> self, void*) {
            if (!m_active)
                return;
            self->updateTimeout(INTERVAL);
            onTick();
        },
        nullptr);
    g_pEventLoopManager->addTimer(m_timer);
}

void CSaverManager::stopTimer() {
    if (!m_timer)
        return;
    m_timer->updateTimeout(std::nullopt);
    if (g_pEventLoopManager)
        g_pEventLoopManager->removeTimer(m_timer);
    m_timer.reset();
}

int CSaverManager::timerArmed() const {
    return m_timer && m_timer->armed() ? 1 : 0;
}

size_t CSaverManager::monitorStates() const {
    return m_mons.size();
}

void CSaverManager::onTick() {
    m_tick++;
    invalidateAll();
}

// ---- rendering ----

void CSaverManager::draw(PHLMONITOR mon) {
    if (!m_active || !mon)
        return;

    const CBox BOX = {0, 0, mon->m_transformedSize.x, mon->m_transformedSize.y};

    const auto& FILTER = m_active->opts.monitors;
    if (!FILTER.empty() && std::ranges::find(FILTER, mon->m_name) == FILTER.end()) {
        CRectPassElement::SRectData data;
        data.box   = BOX;
        data.color = CAbyssColor(0.F, 0.F, 0.F, 1.F);
        g_pAbyssRenderer->m_renderPass.add(makeUnique<CRectPassElement>(data));
        return;
    }

    auto& st = m_mons[mon->m_id];
    if (!st.tex || st.tick != m_tick || st.size != mon->m_transformedSize || st.scale != mon->m_scale) {
        const int W = std::max(1, sc<int>(mon->m_transformedSize.x));
        const int H = std::max(1, sc<int>(mon->m_transformedSize.y));

        auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
        auto* cr      = cairo_create(surface);
        cairo_scale(cr, mon->m_scale, mon->m_scale);

        abyss::SaverFrame frame;
        frame.width   = W / mon->m_scale;
        frame.height  = H / mon->m_scale;
        frame.scale   = mon->m_scale;
        frame.monitor = mon->m_name;
        frame.time    = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_start).count();
        frame.now     = std::chrono::system_clock::now();
        if (m_active->opts.audio)
            frame.audio = std::span<const float>(m_audio);

        abyss::Canvas canvas(cr);
        bool          ok = false;
        try {
            ok = m_active->draw && m_active->draw(canvas, frame);
        } catch (const std::exception& e) { Log::logger->log(Log::ERR, "[saver] exception in screensaver callback: {}", e.what()); } catch (...) {
            Log::logger->log(Log::ERR, "[saver] unknown exception in screensaver callback");
        }
        if (!ok) {
            // the callback failed: black, and keep going
            cairo_identity_matrix(cr);
            cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
            cairo_set_source_rgba(cr, 0, 0, 0, 1);
            cairo_paint(cr);
        }

        cairo_surface_flush(surface);
        st.tex = g_pAbyssRenderer->createTexture(surface);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);

        st.size  = mon->m_transformedSize;
        st.scale = mon->m_scale;
        st.tick  = m_tick;
    }

    if (!st.tex)
        return;

    CTexPassElement::SRenderData data;
    data.tex = st.tex;
    data.box = BOX;
    data.a   = 1.F;
    g_pAbyssRenderer->m_renderPass.add(makeUnique<CTexPassElement>(std::move(data)));
}

// ---- input ----

bool CSaverManager::consumeKey(uint32_t keycode, bool pressed) {
    const bool WAS = active();
    if (WAS)
        hide();
    if (pressed)
        return WAS && m_swallowedKeys.insert(keycode).second;
    return m_swallowedKeys.erase(keycode) > 0;
}

bool CSaverManager::consumeButton(uint32_t button, bool pressed) {
    const bool WAS = active();
    if (WAS)
        hide();
    if (pressed)
        return WAS && m_swallowedButtons.insert(button).second;
    return m_swallowedButtons.erase(button) > 0;
}

bool CSaverManager::consumeOther() {
    if (!active())
        return false;
    hide();
    return true;
}

bool CSaverManager::consumeTouchDown(int32_t id) {
    const bool WAS = active();
    if (WAS)
        hide();
    // a down of a finger that is still marked (lost up) starts over
    m_swallowedTouches.erase(id);
    if (WAS)
        m_swallowedTouches.insert(id);
    return WAS;
}

bool CSaverManager::touchSwallowed(int32_t id, bool up) {
    if (up)
        return m_swallowedTouches.erase(id) > 0;
    return m_swallowedTouches.contains(id);
}

void CSaverManager::touchCancel() {
    m_swallowedTouches.clear();
}

bool CSaverManager::consumeGestureBegin() {
    const bool WAS = active();
    if (WAS)
        hide();
    m_swallowedGesture = WAS; // every begin resets it, so a lost end cannot stick
    return WAS;
}

bool CSaverManager::gestureSwallowed(bool end) {
    const bool S = m_swallowedGesture;
    if (end)
        m_swallowedGesture = false;
    return S;
}

bool CSaverManager::consumeTablet(const void* dev, uint32_t code, bool pressed) {
    const bool WAS = active();
    if (WAS)
        hide();
    const auto KEY = std::make_pair(dev, code);
    if (pressed)
        return WAS && m_swallowedTablet.insert(KEY).second;
    return m_swallowedTablet.erase(KEY) > 0;
}

// ---- cava ----

std::string CSaverManager::cavaConfigPath() const {
    const char* rt = std::getenv("XDG_RUNTIME_DIR");
    return std::string(rt ? rt : "/tmp") + "/abyss/saver-cava.conf";
}

bool CSaverManager::cavaRunning() const {
    return m_cavaPid > 0;
}

void CSaverManager::startCava() {
    if (m_cavaPid > 0)
        return;

    const int BARS = std::clamp(m_active->opts.bars, 1, 512);
    const int FPS  = std::clamp(m_active->opts.fps > 0 ? m_active->opts.fps : 30, 1, 144);

    m_cavaConf = cavaConfigPath();
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(m_cavaConf).parent_path(), ec);
    {
        std::ofstream f(m_cavaConf, std::ios::trunc);
        if (!f) {
            Log::logger->log(Log::ERR, "[saver] cannot write {}", m_cavaConf);
            return;
        }
        f << std::format("[general]\nbars = {}\nframerate = {}\n\n[output]\nmethod = raw\nraw_target = /dev/stdout\ndata_format = binary\nbit_format = 16bit\nchannels = mono\n", BARS, FPS);
    }

    int fds[2];
    if (pipe2(fds, O_CLOEXEC | O_NONBLOCK) != 0) {
        Log::logger->log(Log::ERR, "[saver] pipe failed: {}", strerror(errno));
        return;
    }

    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, fds[1], STDOUT_FILENO); // dup2 clears CLOEXEC on the new fd
    posix_spawn_file_actions_addopen(&fa, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&fa, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

    const char* argv[] = {"cava", "-p", m_cavaConf.c_str(), nullptr};
    pid_t       pid    = -1;
    const int   RC     = posix_spawnp(&pid, "cava", &fa, nullptr, const_cast<char* const*>(argv), environ);
    posix_spawn_file_actions_destroy(&fa);
    close(fds[1]);

    if (RC != 0) {
        Log::logger->log(Log::WARN, "[saver] cannot start cava ({}), the bars stay quiet", strerror(RC));
        close(fds[0]);
        return;
    }

    m_cavaPid    = pid;
    m_cavaPidfd  = pidfdOpen(pid);
    m_cavaFd     = fds[0];
    m_cavaSource = g_pEventLoopManager->addFdWatcher(m_cavaFd, &CSaverManager::cavaFdCallback, this);
    m_cavaBuf.clear();
    Log::logger->log(Log::DEBUG, "[saver] cava started (pid {})", pid);
}

void CSaverManager::stopCava() {
    if (m_cavaSource) {
        wl_event_source_remove(m_cavaSource);
        m_cavaSource = nullptr;
    }
    if (m_cavaFd >= 0) {
        close(m_cavaFd);
        m_cavaFd = -1;
    }
    m_cavaBuf.clear();
    std::ranges::fill(m_audio, 0.F);

    if (m_cavaPid <= 0)
        return;

    const pid_t PID    = m_cavaPid;
    const int   PIDFD  = m_cavaPidfd;
    m_cavaPid          = -1;
    m_cavaPidfd        = -1;
    pidfdSignal(PIDFD, PID, SIGTERM);

    if (m_exiting || (g_pCompositor && g_pCompositor->m_isShuttingDown)) {
        // no event loop to wait for: give it a moment, then make sure
        for (int i = 0; i < 30 && !pidfdExited(PIDFD); i++)
            usleep(10000);
        if (!pidfdExited(PIDFD))
            pidfdSignal(PIDFD, PID, SIGKILL);
        if (PIDFD >= 0)
            close(PIDFD);
        return;
    }

    // fallback: SIGKILL if SIGTERM was ignored
    std::erase_if(m_killTimers, [](const auto& t) { return !t->armed(); });
    auto kt = makeShared<CEventLoopTimer>(
        std::chrono::milliseconds(300),
        [PID, PIDFD](SP<CEventLoopTimer> self, void*) {
            if (!pidfdExited(PIDFD))
                pidfdSignal(PIDFD, PID, SIGKILL);
            if (PIDFD >= 0)
                close(PIDFD);
        },
        nullptr);
    g_pEventLoopManager->addTimer(kt);
    m_killTimers.emplace_back(std::move(kt));
}

int CSaverManager::cavaFdCallback(int fd, uint32_t mask, void* data) {
    sc<CSaverManager*>(data)->onCavaReadable();
    return 0;
}

void CSaverManager::onCavaReadable() {
    if (m_cavaFd < 0)
        return;

    uint8_t buf[8192];
    bool    eof = false;
    for (;;) {
        const auto N = read(m_cavaFd, buf, sizeof(buf));
        if (N > 0) {
            m_cavaBuf.insert(m_cavaBuf.end(), buf, buf + N);
            continue;
        }
        if (N == 0)
            eof = true;
        else if (errno == EINTR)
            continue;
        break; // EAGAIN or error
    }

    // newest complete frame: BARS * uint16
    const size_t BARS  = m_audio.size();
    const size_t FRAME = BARS * 2;
    if (FRAME > 0 && m_cavaBuf.size() >= FRAME) {
        const size_t COMPLETE = (m_cavaBuf.size() / FRAME) * FRAME;
        const auto*  LAST     = m_cavaBuf.data() + COMPLETE - FRAME;
        for (size_t i = 0; i < BARS; i++) {
            uint16_t v;
            std::memcpy(&v, LAST + i * 2, 2);
            m_audio[i] = std::clamp(v / 65535.F, 0.F, 1.F);
        }
        m_cavaBuf.erase(m_cavaBuf.begin(), m_cavaBuf.begin() + COMPLETE);
    }

    if (eof) {
        // cava died (or was killed from outside): quiet bars, the screensaver goes on
        Log::logger->log(Log::WARN, "[saver] cava exited");
        if (m_cavaSource) {
            wl_event_source_remove(m_cavaSource);
            m_cavaSource = nullptr;
        }
        close(m_cavaFd);
        m_cavaFd = -1;
        if (m_cavaPidfd >= 0)
            close(m_cavaPidfd);
        m_cavaPidfd = -1;
        m_cavaPid   = -1;
        std::ranges::fill(m_audio, 0.F);
    }
}
