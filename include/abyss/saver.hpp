// Screensaver API (part of c.idle, see abyss/config.hpp).
//
//   c.idle(300, screensaver([](Canvas& g, const SaverFrame& f) {
//       g.clear(0x000000);
//       g.text(f.width / 2, f.height / 2, "hello", {.size = 64, .color = 0xc8a25a, .align = Align::Center});
//   }, {.fps = 30}));
//   c.idle(300, screensaver(saver::clock()));
//   c.idle(300, screensaver(saver::cava({.bars = 64, .color = 0x80b0d8})));
//
// The callback is called once per monitor and frame while the screensaver is shown (not at all otherwise). It draws into a Canvas,
// a thin wrapper around cairo/pango: no widgets, no toolkit. Colors are 0xRRGGBB (an optional alpha is a separate 0..1 argument).
// Coordinates are logical pixels (SaverFrame::width/height already divided by the monitor scale), origin top left.
// An exception in the callback is caught and logged, the frame stays black.
//
// saver::clock() and saver::cava() are ordinary functions below, built from nothing but this public API: use them as examples for your own.
#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <functional>
#include <span>
#include <string>
#include <vector>

// raw cairo access (Canvas::cairo()): include <cairo.h> yourself when you use it
typedef struct _cairo cairo_t;

namespace abyss {

    enum class Align {
        Left,   // x is the left edge of the text
        Center, // x is the middle of the text
        Right,  // x is the right edge of the text
    };

    struct TextOpts {
        int         size  = 24;       // font size in logical pixels
        uint32_t    color = 0xffffff; // 0xRRGGBB
        Align       align = Align::Left;
        double      alpha = 1.0;
        bool        bold  = false;
        std::string font; // Pango family, empty = "IBM Plex Sans"
    };

    // Drawing surface of one monitor. Created by the compositor, only valid inside the screensaver callback.
    class Canvas {
      public:
        // fills everything (also replaces what was drawn before, like the CLEAR operator)
        void clear(uint32_t color, double alpha = 1.0);
        void rect(double x, double y, double w, double h, uint32_t color, double alpha = 1.0);
        void roundRect(double x, double y, double w, double h, double radius, uint32_t color, double alpha = 1.0);
        void line(double x1, double y1, double x2, double y2, uint32_t color, double width = 1.0, double alpha = 1.0);
        void circle(double cx, double cy, double radius, uint32_t color, double alpha = 1.0);
        // y is the vertical middle of the (single or multi line) text. Pango markup is NOT interpreted.
        void text(double x, double y, const std::string& str, const TextOpts& opts = {});
        // width and height of the text as text() would draw it
        std::pair<double, double> textSize(const std::string& str, const TextOpts& opts = {});
        // PNG only. w / h = 0 keeps the size of the image (one of them 0 keeps the aspect ratio); the image is cached until the screensaver ends
        void image(const std::string& path, double x, double y, double w = 0, double h = 0);
        // the raw cairo context (already scaled to logical pixels)
        cairo_t* cairo() const {
            return m_cr;
        }

        // internal
        explicit Canvas(cairo_t* cr) : m_cr(cr) {}
        Canvas(const Canvas&)            = delete;
        Canvas& operator=(const Canvas&) = delete;

      private:
        cairo_t* m_cr = nullptr;
    };

    struct SaverFrame {
        double      width  = 0; // logical size of the monitor
        double      height = 0;
        double      scale  = 1; // monitor scale (1 logical pixel = scale device pixels)
        std::string monitor;    // output name
        double      time = 0;   // seconds since the screensaver started
        std::chrono::system_clock::time_point now;
        // current Cava bars, 0..1, SaverOpts::bars entries. Empty when SaverOpts::audio is off. Quiet bars (all 0) when no audio plays or cava is missing.
        std::span<const float> audio;
    };

    using SaverFn = std::function<void(Canvas&, const SaverFrame&)>;

    struct SaverOpts {
        int                      fps   = 30;    // redraw rate, 0 = draw once (and again only when the monitor changes), max 144
        bool                     audio = false; // start cava and fill SaverFrame::audio while the screensaver runs
        int                      bars  = 64;    // number of audio bars (1..512), only with audio
        std::vector<std::string> monitors;      // only these outputs (empty = all; the others stay black)
    };

    struct Screensaver {
        SaverFn   draw;
        SaverOpts opts;
    };

    // c.idle(seconds, screensaver(fn, opts)): shows fn on every monitor when idle, hides it (and swallows that first input) on resume.
    inline Screensaver screensaver(SaverFn fn, SaverOpts opts = {}) {
        return Screensaver{std::move(fn), std::move(opts)};
    }
    inline Screensaver screensaver(Screensaver s) {
        return s;
    }

    namespace saver {

        struct ClockOpts {
            uint32_t    color      = 0xffffff;
            int         size       = 160; // time font size in logical pixels
            std::string format     = "%H:%M"; // strftime
            std::string dateFormat = "%A, %d. %B %Y"; // strftime, empty = no date line
            uint32_t    background = 0x000000;
        };

        // big clock with date, 1 Hz
        inline Screensaver clock(ClockOpts o = {}) {
            return Screensaver{[o](Canvas& g, const SaverFrame& f) {
                                   const auto t = std::chrono::system_clock::to_time_t(f.now);
                                   std::tm    tm{};
                                   localtime_r(&t, &tm);
                                   auto fmt = [&](const std::string& format) {
                                       char buf[128];
                                       return std::string(buf, std::strftime(buf, sizeof(buf), format.c_str(), &tm));
                                   };
                                   g.clear(o.background);
                                   const double mid = f.height / 2 - (o.dateFormat.empty() ? 0 : o.size * 0.18);
                                   g.text(f.width / 2, mid, fmt(o.format), {.size = o.size, .color = o.color, .align = Align::Center});
                                   if (!o.dateFormat.empty())
                                       g.text(f.width / 2, mid + o.size * 0.7, fmt(o.dateFormat),
                                              {.size = std::max(12, o.size / 5), .color = o.color, .align = Align::Center, .alpha = 0.7});
                               },
                               {.fps = 1}};
        }

        struct CavaOpts {
            int      bars       = 64;
            uint32_t color      = 0x80b0d8;
            uint32_t background = 0x000000;
            int      fps        = 60;
        };

        // audio spectrum bars (needs the `cava` program, otherwise the bars stay quiet)
        inline Screensaver cava(CavaOpts o = {}) {
            return Screensaver{[o](Canvas& g, const SaverFrame& f) {
                                   g.clear(o.background);
                                   const int n = std::max<int>(1, o.bars);
                                   const double slot = f.width * 0.9 / n, x0 = f.width * 0.05, base = f.height * 0.75, maxH = f.height * 0.55;
                                   for (int i = 0; i < n; i++) {
                                       const double v = i < (int)f.audio.size() ? std::clamp<double>(f.audio[i], 0.0, 1.0) : 0.0;
                                       const double h = std::max(3.0, v * maxH);
                                       g.roundRect(x0 + i * slot + slot * 0.15, base - h, slot * 0.7, h, std::min(slot * 0.35, 6.0), o.color);
                                   }
                               },
                               {.fps = o.fps, .audio = true, .bars = o.bars}};
        }

    } // namespace saver

} // namespace abyss
