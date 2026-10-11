// Example of a full-power layout: "centered master". The first window sits in the middle, the others are split between a left
// and a right column. Message: `abyssctl dispatch layoutmsg "ratio +0.05"` (or "ratio 0.6") changes the width of the middle window.
//
// Use it from your config.hpp:
//     #include "/path/to/Abyssland/examples/centered-master.hpp"     // or copy the class into your config
//     ... inside configure():   c.addLayout<CenteredMaster>("centered-master");
//     c.general.layout = "centered-master";
#pragma once

#include <abyss/config.hpp>

#include <algorithm>
#include <charconv>

struct CenteredMaster : abyss::TiledLayout {
    // per-workspace state: one instance of this class exists for every workspace that uses the layout
    double m_ratio = 0.5;

    void recalculate() override {
        const auto T = targets();
        const auto A = workArea();
        if (T.empty())
            return;

        // one window: everything; two: master + one side column
        if (T.size() == 1) {
            place(T[0], A);
            return;
        }

        const bool   twoOnly = T.size() == 2;
        const double masterW = A.w * m_ratio;
        const double masterX = twoOnly ? A.x : A.x + (A.w - masterW) / 2.0;
        place(T[0], {masterX, A.y, masterW, A.h});

        // the rest alternates: right column first, then left
        std::vector<abyss::LayoutTarget> right, left;
        for (size_t i = 1; i < T.size(); ++i)
            ((i % 2 == 1) ? right : left).push_back(T[i]);

        const double sideW  = (A.w - masterW) / (twoOnly ? 1.0 : 2.0);
        auto         stack = [&](const std::vector<abyss::LayoutTarget>& col, double x) {
            for (size_t i = 0; i < col.size(); ++i)
                place(col[i], {x, A.y + A.h * double(i) / double(col.size()), sideW, A.h / double(col.size())});
        };
        stack(right, masterX + masterW);
        stack(left, A.x);
    }

    // dragging a border resizes the master
    void resizeTarget(abyss::Vec2 delta, const abyss::LayoutTarget&, abyss::Corner) override {
        const auto A = workArea();
        if (A.w > 0)
            m_ratio = std::clamp(m_ratio + delta.x / A.w, 0.2, 0.9);
        recalculate();
    }

    std::string layoutMsg(std::string_view msg) override {
        if (!msg.starts_with("ratio "))
            return "unknown message, try: ratio <0.2..0.9 | +x | -x>";

        std::string_view arg = msg.substr(6);
        const bool       rel = arg.starts_with('+') || arg.starts_with('-');
        double           v   = 0;
        const char*      b   = arg.data() + (arg.starts_with('+') ? 1 : 0);
        const auto [p, ec]   = std::from_chars(b, arg.data() + arg.size(), v);
        if (ec != std::errc{} || p != arg.data() + arg.size())
            return "ratio expects a number";

        m_ratio = std::clamp(rel ? m_ratio + v : v, 0.2, 0.9);
        recalculate();
        return "";
    }
};
