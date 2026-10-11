// Abysslang layouts: your own tiling layouts, written in the config. There are two ways.
//
// 1) Simple: a function that places boxes. No state, no resize / swap logic of its own.
//
//      c.addLayout("grid", [](LayoutCtx& l) {
//          auto cells = l.grid(l.count());
//          for (size_t i = 0; i < l.count(); ++i)
//              l.place(i, cells[i]);
//      });
//
//    Optional hooks (observers that run in addition to the built-in behavior) for layout messages, resizing and swapping:
//
//      c.addLayout("columns", recalc, LayoutHooks{.onMsg = [](LayoutCtx&, std::string_view msg) -> std::string { ...; return ""; }});
//
// 2) Full power: derive from abyss::TiledLayout and override whatever you want (recalculate is the only thing you must
//    implement). One instance exists per workspace, so member variables are per-workspace state (split ratios, ...).
//
//      struct MyAlgo : TiledLayout {
//          void        recalculate() override { for (auto& t : targets()) place(t, box); }
//          std::string layoutMsg(std::string_view msg) override { ...; return ""; }   // "" = ok, else an error message
//      };
//      c.addLayout<MyAlgo>("my-layout");
//
//    See examples/centered-master.hpp for a complete one.
//
// Select a layout with c.general.layout = "grid" (every workspace), with c.workspaceRule("3", {.layout = "grid"}) (one workspace),
// or at runtime with `abyssctl dispatch setlayout grid` (all workspaces without a workspace-rule layout).
// `abyssctl dispatch layoutmsg <text>` is delivered to the active workspace's layout (onMsg / TiledLayout::layoutMsg).
// If a layout disappears from the config (reload), its workspaces fall back to dwindle and keep all their windows.
//
// The API only uses abyss types plus PHLWINDOW (the window handle used everywhere in abyss/actions.hpp).
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "desktop/DesktopTypes.hpp"
#include "helpers/memory/Memory.hpp"

#include <abyss/actions.hpp>
#include <abyss/types.hpp>

namespace Layout {
    class ITarget;
}

namespace abyss {

    struct Box {
        double x = 0, y = 0, w = 0, h = 0;

        double right() const {
            return x + w;
        }
        double bottom() const {
            return y + h;
        }
        // the box shrunk by `px` on every side (never negative)
        Box inset(double px) const {
            const double nw = w - 2 * px, nh = h - 2 * px;
            return {x + px, y + px, nw < 0 ? 0 : nw, nh < 0 ? 0 : nh};
        }
    };

    enum class Side {
        Left,
        Right,
        Top,
        Bottom
    };

    // which edges / corner a resize drags: a combination of these bits (e.g. CornerTop | CornerLeft); CornerNone: not interactive
    enum Corner : uint8_t {
        CornerNone   = 0,
        CornerTop    = 1 << 0,
        CornerBottom = 1 << 1,
        CornerLeft   = 1 << 2,
        CornerRight  = 1 << 3,
    };

    // One tiled window as seen by a layout: a cheap value you can copy around. Two handles of the same window compare equal.
    struct LayoutTarget {
        size_t                index    = 0;       // position in the layout order (SIZE_MAX if not in the list)
        PHLWINDOW             window;             // the window (null only for exotic targets)
        Box                   box;                // where it was when the handle was made
        bool                  floating = false;
        SP<::Layout::ITarget> target;             // internal

        // puts the window into `b` (negative sizes become 0); compositor gaps and borders are applied on top
        void place(const Box& b) const;
        bool operator==(const LayoutTarget& o) const {
            return target == o.target;
        }
    };

    // ---- simple API ----

    // What the layout function sees and does. Valid only during the call.
    class LayoutCtx {
      public:
        Box                       area;          // work area of the workspace (global coordinates): monitor minus bars / reserved areas
        std::vector<LayoutTarget> targets;       // the tiled windows, in order
        int64_t                   workspace = 0; // workspace id
        std::string_view          message;       // in onMsg: the layout message text; empty otherwise

        size_t count() const {
            return targets.size();
        }

        // Tiles `n` cells into area: ceil(sqrt(n)) columns, ceil(n / cols) rows, the last row is stretched over the full width.
        std::vector<Box> grid(size_t n) const;
        // cell `i` (0-based) of a regular `cols` x `rows` grid inside area; rows = 0: as many rows as needed for count() cells
        Box              gridCell(size_t i, int cols, int rows = 0) const;
        // the i-th (0-based) of n equally wide columns / equally tall rows of area
        Box              column(size_t i, size_t n) const;
        Box              row(size_t i, size_t n) const;
        // `ratio` (0..1) of `b` from the given side
        static Box       split(const Box& b, Side side, double ratio);

        // put window i (0-based) into box, same as targets[i].place(box)
        void             place(size_t i, const Box& box);
    };

    using LayoutFn = std::function<void(LayoutCtx&)>;

    // Optional extras for the simple API. Hooks are observers: the built-in behavior (reordering the window list on swap /
    // move, re-running the layout function after every change) always happens, the hook is called in addition.
    struct LayoutHooks {
        // `abyssctl dispatch layoutmsg <msg>`. Return "" for success, or an error message.
        std::function<std::string(LayoutCtx&, std::string_view msg)> onMsg;
        // window `index` is being resized by delta (pixels); the layout is re-run after
        std::function<void(LayoutCtx&, size_t index, Vec2 delta)> onResize;
        // windows a and b (0-based indices, in the new order) were swapped or moved by the user; the layout is re-run after
        std::function<void(LayoutCtx&, size_t a, size_t b)> onSwap;
    };

    // ---- full-power API ----

    // Base class for custom layouts. One instance exists per workspace using the layout, so members are per-workspace state.
    // Default behavior: keeps the tiled windows in a list, calls recalculate() after every change, swap / move-in-direction
    // reorder the list, getNextCandidate cycles through the list. Override whatever you need; only recalculate() is required.
    class TiledLayout {
      public:
        virtual ~TiledLayout() = default;

        // a window entered / moved in from another workspace / left (call TiledLayout:: to keep the default list handling)
        virtual void newTarget(const LayoutTarget& t);
        virtual void movedTarget(const LayoutTarget& t, std::optional<Vec2> focalPoint = std::nullopt);
        virtual void removeTarget(const LayoutTarget& t);

        // REQUIRED: position all windows (targets() / place())
        virtual void recalculate() = 0;

        // a window is being resized by delta pixels (default: just recalculate)
        virtual void resizeTarget(Vec2 delta, const LayoutTarget& t, Corner corner);
        // `swapwindow` / drag-swap, `movewindow l|r|u|d`
        virtual void swapTargets(const LayoutTarget& a, const LayoutTarget& b);
        virtual void moveTargetInDirection(const LayoutTarget& t, Dir dir, bool silent);
        // `abyssctl dispatch layoutmsg <msg>`: return "" if handled, an error message otherwise (default: ignore)
        virtual std::string layoutMsg(std::string_view msg);
        // which window gets focus when `old` goes away (default: the next one in the list)
        virtual std::optional<LayoutTarget> getNextCandidate(const LayoutTarget& old);
        // size a new window opens with (default: the work area)
        virtual std::optional<Vec2> predictSizeForNewTarget();

        // ---- helpers ----
        // the live tiled windows, in order
        std::vector<LayoutTarget> targets() const;
        // the workspace's usable area (global coordinates); empty box if the layout is not attached
        Box                       workArea() const;
        int64_t                   workspaceId() const;
        // same as t.place(box)
        static void               place(const LayoutTarget& t, const Box& box);

      protected:
        // the list order used by the defaults; edit it freely in your overrides
        std::vector<WP<::Layout::ITarget>> m_list;

      private:
        friend struct TiledLayoutAccess;
        std::function<Box()>     m_areaFn;
        std::function<int64_t()> m_workspaceFn;
    };

    namespace detail {
        // implemented in the compositor (AbyssLayout.cpp)
        bool registerLayout(std::string name, std::function<UP<TiledLayout>()> factory);
        bool registerSimpleLayout(std::string name, LayoutFn fn, LayoutHooks hooks);
        // switches every workspace without a workspace-rule layout (the `setlayout` dispatcher / setLayout action)
        std::string switchLayout(const std::string& name);
    }

} // namespace abyss
