// Abysslang layouts (include/abyss/layout.hpp): TiledLayout base class, the simple function adapter and the registration glue.
// Not `using namespace abyss`: see Abysslang.cpp.

#include <abyss/config.hpp>

#include <algorithm>
#include <cmath>
#include <format>

#include "ConfigManager.hpp"

#include "../../debug/log/Logger.hpp"
#include "../../desktop/view/Window.hpp"
#include "../../layout/algorithm/Algorithm.hpp"
#include "../../layout/algorithm/TiledAlgorithm.hpp"
#include "../../layout/space/Space.hpp"
#include "../../layout/target/Target.hpp"
#include "../../layout/supplementary/WorkspaceAlgoMatcher.hpp"

using namespace abyss;

namespace {
    ::Config::Native::CConfigManager* M() {
        return ::Config::Native::self();
    }

    void reportError(const std::string& msg) {
        Log::logger->log(Log::ERR, "[abyss] {}", msg);
        if (auto* m = M())
            m->addError(std::string{msg});
    }

    CBox toCBox(const Box& b) {
        return CBox{b.x, b.y, b.w, b.h}.noNegativeSize();
    }

    Box fromCBox(const CBox& b) {
        return {b.x, b.y, b.w, b.h};
    }
}

// ------------------------------------------------------------------------------------------------ LayoutCtx

void LayoutTarget::place(const Box& b) const {
    if (target)
        target->setPositionGlobal(toCBox(b));
}

std::vector<Box> LayoutCtx::grid(size_t n) const {
    std::vector<Box> out;
    if (n == 0)
        return out;

    const size_t cols = std::max<size_t>(1, sc<size_t>(std::ceil(std::sqrt(sc<double>(n)))));
    const size_t rows = (n + cols - 1) / cols;

    for (size_t i = 0; i < n; ++i) {
        const size_t r = i / cols, c = i % cols;
        // the last row may have fewer cells: stretch them over the full width
        const size_t inRow = (r == rows - 1) ? n - r * cols : cols;
        out.push_back({area.x + area.w * c / inRow, area.y + area.h * r / rows, area.w / inRow, area.h / rows});
    }
    return out;
}

Box LayoutCtx::gridCell(size_t i, int cols, int rows) const {
    const int w = std::max(1, cols);
    const int h = rows > 0 ? rows : std::max(1, sc<int>(std::ceil(sc<double>(std::max<size_t>(1, count())) / sc<double>(w))));
    const int r = sc<int>(i) / w, c = sc<int>(i) % w;
    return {area.x + area.w * c / w, area.y + area.h * r / h, area.w / w, area.h / h};
}

Box LayoutCtx::column(size_t i, size_t n) const {
    n = std::max<size_t>(1, n);
    return {area.x + area.w * sc<double>(i) / sc<double>(n), area.y, area.w / sc<double>(n), area.h};
}

Box LayoutCtx::row(size_t i, size_t n) const {
    n = std::max<size_t>(1, n);
    return {area.x, area.y + area.h * sc<double>(i) / sc<double>(n), area.w, area.h / sc<double>(n)};
}

Box LayoutCtx::split(const Box& b, Side side, double ratio) {
    ratio = std::clamp(ratio, 0.0, 1.0);
    switch (side) {
        case Side::Left: return {b.x, b.y, b.w * ratio, b.h};
        case Side::Right: return {b.x + b.w * (1.0 - ratio), b.y, b.w * ratio, b.h};
        case Side::Top: return {b.x, b.y, b.w, b.h * ratio};
        case Side::Bottom: return {b.x, b.y + b.h * (1.0 - ratio), b.w, b.h * ratio};
    }
    return b;
}

void LayoutCtx::place(size_t i, const Box& box) {
    if (i >= targets.size())
        return;
    targets[i].place(box);
    targets[i].box = box;
}

// ------------------------------------------------------------------------------------------------ LayoutTarget / TiledLayout

namespace abyss {
    // gives the compositor-side adapter access to TiledLayout's private hooks
    struct TiledLayoutAccess {
        static void attach(TiledLayout& l, std::function<Box()> area, std::function<int64_t()> ws) {
            l.m_areaFn      = std::move(area);
            l.m_workspaceFn = std::move(ws);
        }
        static std::vector<WP<::Layout::ITarget>>& list(TiledLayout& l) {
            return l.m_list;
        }
    };
}

namespace {
    abyss::LayoutTarget handleFor(const SP<::Layout::ITarget>& t, size_t index = SIZE_MAX) {
        LayoutTarget lt;
        lt.index    = index;
        lt.window   = t ? t->window() : nullptr;
        lt.box      = t ? fromCBox(t->position()) : Box{};
        lt.floating = lt.window && lt.window->m_isFloating;
        lt.target   = t;
        return lt;
    }
}

void TiledLayout::newTarget(const LayoutTarget& t) {
    m_list.emplace_back(t.target);
    recalculate();
}

void TiledLayout::movedTarget(const LayoutTarget& t, std::optional<Vec2>) {
    TiledLayout::newTarget(t);
}

void TiledLayout::removeTarget(const LayoutTarget& t) {
    std::erase_if(m_list, [&t](const auto& w) { return !w.lock() || w.lock() == t.target; });
    recalculate();
}

void TiledLayout::resizeTarget(Vec2, const LayoutTarget&, Corner) {
    recalculate();
}

void TiledLayout::swapTargets(const LayoutTarget& a, const LayoutTarget& b) {
    auto ia = std::ranges::find_if(m_list, [&a](const auto& w) { return w.lock() == a.target; });
    auto ib = std::ranges::find_if(m_list, [&b](const auto& w) { return w.lock() == b.target; });

    if (ia != m_list.end() && ib != m_list.end())
        std::iter_swap(ia, ib);
    else {
        if (ia != m_list.end())
            *ia = b.target;
        if (ib != m_list.end())
            *ib = a.target;
    }

    recalculate();
}

void TiledLayout::moveTargetInDirection(const LayoutTarget& t, Dir dir, bool) {
    auto it = std::ranges::find_if(m_list, [&t](const auto& w) { return w.lock() == t.target; });
    if (it == m_list.end())
        return;

    if ((dir == Dir::Left || dir == Dir::Up) && it != m_list.begin())
        std::iter_swap(it, std::prev(it));
    else if ((dir == Dir::Right || dir == Dir::Down) && std::next(it) != m_list.end())
        std::iter_swap(it, std::next(it));
    else
        return;

    recalculate();
}

std::string TiledLayout::layoutMsg(std::string_view) {
    return "";
}

std::optional<LayoutTarget> TiledLayout::getNextCandidate(const LayoutTarget& old) {
    const auto ALL = targets();
    if (ALL.empty())
        return std::nullopt;

    auto it = std::ranges::find(ALL, old);
    if (it == ALL.end())
        return ALL.back();
    if (ALL.size() == 1)
        return std::nullopt;

    auto next = std::next(it);
    if (next == ALL.end())
        next = ALL.begin();
    return *next;
}

std::optional<Vec2> TiledLayout::predictSizeForNewTarget() {
    if (!m_areaFn)
        return std::nullopt;
    const auto A = m_areaFn();
    return Vec2{sc<float>(A.w), sc<float>(A.h)};
}

std::vector<LayoutTarget> TiledLayout::targets() const {
    std::vector<LayoutTarget> out;
    for (const auto& w : m_list) {
        if (const auto L = w.lock())
            out.emplace_back(handleFor(L, out.size()));
    }
    return out;
}

Box TiledLayout::workArea() const {
    return m_areaFn ? m_areaFn() : Box{};
}

int64_t TiledLayout::workspaceId() const {
    return m_workspaceFn ? m_workspaceFn() : 0;
}

void TiledLayout::place(const LayoutTarget& t, const Box& box) {
    t.place(box);
}

// ------------------------------------------------------------------------------------------------ compositor-side adapter

namespace {
    Corner toCorner(::Layout::eRectCorner c) {
        return sc<Corner>(sc<uint8_t>(c)); // same bit layout (checked below)
    }
    static_assert(::Layout::CORNER_TOP == CornerTop && ::Layout::CORNER_BOTTOM == CornerBottom && ::Layout::CORNER_LEFT == CornerLeft && ::Layout::CORNER_RIGHT == CornerRight);

    // wraps a user TiledLayout as the compositor's tiled algorithm
    class CUserTiledAlgorithm : public ::Layout::ITiledAlgorithm {
      public:
        CUserTiledAlgorithm(std::string name, UP<TiledLayout>&& user) : m_name(std::move(name)), m_user(std::move(user)) {
            TiledLayoutAccess::attach(
                *m_user,
                [this] {
                    const auto P = m_parent.lock();
                    const auto S = P ? P->space() : nullptr;
                    return S ? fromCBox(S->workArea()) : Box{};
                },
                [this]() -> int64_t {
                    const auto P = m_parent.lock();
                    const auto S = P ? P->space() : nullptr;
                    return S && S->workspace() ? S->workspace()->m_id : 0;
                });
        }

        void newTarget(SP<::Layout::ITarget> t) override {
            m_user->newTarget(handleFor(t));
        }
        void movedTarget(SP<::Layout::ITarget> t, std::optional<Vector2D> focalPoint) override {
            m_user->movedTarget(handleFor(t), focalPoint ? std::optional<Vec2>{Vec2{sc<float>(focalPoint->x), sc<float>(focalPoint->y)}} : std::nullopt);
        }
        void removeTarget(SP<::Layout::ITarget> t) override {
            m_user->removeTarget(handleFor(t));
        }
        void resizeTarget(const Vector2D& d, SP<::Layout::ITarget> t, ::Layout::eRectCorner c) override {
            m_user->resizeTarget(Vec2{sc<float>(d.x), sc<float>(d.y)}, handleFor(t), toCorner(c));
        }
        void recalculate(::Layout::eRecalculateReason) override {
            m_user->recalculate();
        }
        void swapTargets(SP<::Layout::ITarget> a, SP<::Layout::ITarget> b) override {
            m_user->swapTargets(handleFor(a), handleFor(b));
        }
        void moveTargetInDirection(SP<::Layout::ITarget> t, Math::eDirection dir, bool silent) override {
            Dir d;
            switch (dir) {
                case Math::DIRECTION_LEFT: d = Dir::Left; break;
                case Math::DIRECTION_RIGHT: d = Dir::Right; break;
                case Math::DIRECTION_UP: d = Dir::Up; break;
                case Math::DIRECTION_DOWN: d = Dir::Down; break;
                default: return;
            }
            m_user->moveTargetInDirection(handleFor(t), d, silent);
        }
        ::Config::ErrorResult layoutMsg(const std::string_view& sv) override {
            const auto ERR = m_user->layoutMsg(sv);
            if (ERR.empty())
                return {};
            return std::unexpected(::Config::SConfigError{std::format("layout {}: {}", m_name, ERR), ::Config::eConfigErrorLevel::ERROR, ::Config::eConfigErrorCode::INVALID_ARGUMENT});
        }
        std::optional<Vector2D> predictSizeForNewTarget() override {
            const auto S = m_user->predictSizeForNewTarget();
            if (!S)
                return std::nullopt;
            return Vector2D{S->x, S->y};
        }
        SP<::Layout::ITarget> getNextCandidate(SP<::Layout::ITarget> old) override {
            const auto N = m_user->getNextCandidate(handleFor(old));
            return N ? N->target : nullptr;
        }
        std::optional<std::string> layoutName() const override {
            return m_name;
        }

      private:
        std::string     m_name;
        UP<TiledLayout> m_user;
    };
}

// ------------------------------------------------------------------------------------------------ simple adapter

namespace {
    struct SSimpleLayout {
        std::string name;
        LayoutFn    fn;
        LayoutHooks hooks;
        bool        didError = false;
    };

    class CSimpleLayout : public TiledLayout {
      public:
        CSimpleLayout(SP<SSimpleLayout> spec) : m_spec(std::move(spec)) {}

        void recalculate() override {
            auto ctx = makeCtx();
            if (ctx.targets.empty())
                return;
            run([&] { m_spec->fn(ctx); }, "recalculate");
        }

        void resizeTarget(Vec2 delta, const LayoutTarget& t, Corner) override {
            if (m_spec->hooks.onResize) {
                auto ctx = makeCtx();
                if (const auto IT = std::ranges::find(ctx.targets, t); IT != ctx.targets.end())
                    run([&] { m_spec->hooks.onResize(ctx, IT->index, delta); }, "onResize");
            }
            recalculate();
        }

        void swapTargets(const LayoutTarget& a, const LayoutTarget& b) override {
            TiledLayout::swapTargets(a, b);
            notifySwap(a, b);
        }

        void moveTargetInDirection(const LayoutTarget& t, Dir dir, bool silent) override {
            const auto BEFORE = targets();
            TiledLayout::moveTargetInDirection(t, dir, silent);
            const auto AFTER = targets();
            // the two windows whose positions in the list changed
            std::vector<size_t> changed;
            for (size_t i = 0; i < BEFORE.size() && i < AFTER.size(); ++i)
                if (!(BEFORE[i] == AFTER[i]))
                    changed.push_back(i);
            if (changed.size() == 2)
                notifySwap(AFTER[changed[0]], AFTER[changed[1]]);
        }

        std::string layoutMsg(std::string_view msg) override {
            if (!m_spec->hooks.onMsg)
                return "";

            auto ctx    = makeCtx();
            ctx.message = msg;

            std::string result;
            run([&] { result = m_spec->hooks.onMsg(ctx, msg); }, "onMsg");
            recalculate();
            return result;
        }

      private:
        SP<SSimpleLayout> m_spec;

        LayoutCtx makeCtx() {
            LayoutCtx ctx;
            ctx.area      = workArea();
            ctx.workspace = workspaceId();
            ctx.targets   = targets();
            return ctx;
        }

        void notifySwap(const LayoutTarget& a, const LayoutTarget& b) {
            if (!m_spec->hooks.onSwap)
                return;
            auto   ctx = makeCtx();
            size_t ia = SIZE_MAX, ib = SIZE_MAX;
            for (const auto& t : ctx.targets) {
                if (t == a)
                    ia = t.index;
                if (t == b)
                    ib = t.index;
            }
            if (ia == SIZE_MAX || ib == SIZE_MAX)
                return;
            run([&] { m_spec->hooks.onSwap(ctx, ia, ib); }, "onSwap");
            recalculate();
        }

        template <typename F>
        void run(const F& f, const char* what) {
            try {
                f();
            } catch (const std::exception& e) {
                fail(what, e.what());
            } catch (...) { fail(what, "unknown exception"); }
        }

        void fail(const char* what, const char* msg) {
            Log::logger->log(Log::ERR, "[abyss] layout {} {}: {}", m_spec->name, what, msg);
            if (m_spec->didError)
                return;
            m_spec->didError = true;
            if (auto* m = M())
                m->addError(std::format("layout {} {}: {}", m_spec->name, what, msg));
        }
    };
}

// ------------------------------------------------------------------------------------------------ registration

bool abyss::detail::registerLayout(std::string name, std::function<UP<TiledLayout>()> factory) {
    if (name.empty()) {
        reportError("addLayout: name must not be empty");
        return false;
    }

    auto wrapped = [name, factory = std::move(factory)]() -> UP<::Layout::ITiledAlgorithm> { return makeUnique<CUserTiledAlgorithm>(name, factory()); };

    if (!::Layout::Supplementary::algoMatcher()->registerTiledAlgo(name, &typeid(CUserTiledAlgorithm), std::move(wrapped))) {
        reportError(std::format("addLayout: a layout named '{}' already exists (built-in or registered twice)", name));
        return false;
    }

    if (auto* m = M())
        m->addLayoutName(name);
    return true;
}

bool abyss::detail::registerSimpleLayout(std::string name, LayoutFn fn, LayoutHooks hooks) {
    if (!fn) {
        reportError(std::format("addLayout: layout '{}' has no function", name));
        return false;
    }

    auto spec   = makeShared<SSimpleLayout>();
    spec->name  = name;
    spec->fn    = std::move(fn);
    spec->hooks = std::move(hooks);

    return registerLayout(std::move(name), [spec] { return UP<TiledLayout>(new CSimpleLayout(spec)); });
}

std::string abyss::detail::switchLayout(const std::string& name) {
    const auto& MATCHER = ::Layout::Supplementary::algoMatcher();
    if (!MATCHER->hasTiledAlgo(name))
        return std::format("unknown layout '{}'", name);

    auto* m = M();
    if (!m || !m->set("general:layout", name))
        return "could not set general:layout";

    MATCHER->updateWorkspaceLayouts();
    return "";
}

void abyss::Config::addLayout(std::string name, LayoutFn fn, LayoutHooks hooks) {
    detail::registerSimpleLayout(std::move(name), std::move(fn), std::move(hooks));
}
