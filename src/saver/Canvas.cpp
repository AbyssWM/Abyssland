// abyss::Canvas (include/abyss/saver.hpp): thin cairo/pango wrapper used by screensaver callbacks.

#include <abyss/saver.hpp>

#include <cmath>
#include <map>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>

namespace {
    void setColor(cairo_t* cr, uint32_t rgb, double alpha) {
        cairo_set_source_rgba(cr, ((rgb >> 16) & 0xff) / 255.0, ((rgb >> 8) & 0xff) / 255.0, (rgb & 0xff) / 255.0, alpha);
    }

    PangoLayout* makeLayout(cairo_t* cr, const std::string& str, const abyss::TextOpts& o) {
        PangoLayout*          layout = pango_cairo_create_layout(cr);
        PangoFontDescription* fd     = pango_font_description_new();
        pango_font_description_set_family(fd, o.font.empty() ? "IBM Plex Sans" : o.font.c_str());
        pango_font_description_set_absolute_size(fd, std::max(1, o.size) * PANGO_SCALE);
        pango_font_description_set_weight(fd, o.bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
        pango_layout_set_font_description(layout, fd);
        pango_font_description_free(fd);
        pango_layout_set_text(layout, str.c_str(), -1);
        return layout;
    }

    // PNG cache: lives until the screensaver ended (cleared by abyss::detail::clearCanvasImages, called when it is hidden)
    std::map<std::string, cairo_surface_t*>& imageCache() {
        static std::map<std::string, cairo_surface_t*> c;
        return c;
    }
}

namespace abyss {
    namespace detail {
        void clearCanvasImages() {
            for (auto& [k, s] : imageCache())
                if (s)
                    cairo_surface_destroy(s);
            imageCache().clear();
        }
    }

    void Canvas::clear(uint32_t color, double alpha) {
        cairo_save(m_cr);
        cairo_set_operator(m_cr, CAIRO_OPERATOR_SOURCE);
        setColor(m_cr, color, alpha);
        cairo_paint(m_cr);
        cairo_restore(m_cr);
    }

    void Canvas::rect(double x, double y, double w, double h, uint32_t color, double alpha) {
        setColor(m_cr, color, alpha);
        cairo_rectangle(m_cr, x, y, w, h);
        cairo_fill(m_cr);
    }

    void Canvas::roundRect(double x, double y, double w, double h, double r, uint32_t color, double alpha) {
        r = std::max(0.0, std::min({r, w / 2, h / 2}));
        cairo_new_sub_path(m_cr);
        cairo_arc(m_cr, x + w - r, y + r, r, -M_PI / 2, 0);
        cairo_arc(m_cr, x + w - r, y + h - r, r, 0, M_PI / 2);
        cairo_arc(m_cr, x + r, y + h - r, r, M_PI / 2, M_PI);
        cairo_arc(m_cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
        cairo_close_path(m_cr);
        setColor(m_cr, color, alpha);
        cairo_fill(m_cr);
    }

    void Canvas::line(double x1, double y1, double x2, double y2, uint32_t color, double width, double alpha) {
        setColor(m_cr, color, alpha);
        cairo_set_line_width(m_cr, width);
        cairo_move_to(m_cr, x1, y1);
        cairo_line_to(m_cr, x2, y2);
        cairo_stroke(m_cr);
    }

    void Canvas::circle(double cx, double cy, double radius, uint32_t color, double alpha) {
        cairo_new_sub_path(m_cr);
        cairo_arc(m_cr, cx, cy, std::max(0.0, radius), 0, 2 * M_PI);
        setColor(m_cr, color, alpha);
        cairo_fill(m_cr);
    }

    std::pair<double, double> Canvas::textSize(const std::string& str, const TextOpts& o) {
        PangoLayout* layout = makeLayout(m_cr, str, o);
        int          w = 0, h = 0;
        pango_layout_get_pixel_size(layout, &w, &h);
        g_object_unref(layout);
        return {w, h};
    }

    void Canvas::text(double x, double y, const std::string& str, const TextOpts& o) {
        PangoLayout* layout = makeLayout(m_cr, str, o);
        int          w = 0, h = 0;
        pango_layout_get_pixel_size(layout, &w, &h);
        const double left = o.align == Align::Center ? x - w / 2.0 : (o.align == Align::Right ? x - w : x);
        setColor(m_cr, o.color, o.alpha);
        cairo_move_to(m_cr, std::round(left), std::round(y - h / 2.0));
        pango_cairo_show_layout(m_cr, layout);
        g_object_unref(layout);
    }

    void Canvas::image(const std::string& path, double x, double y, double w, double h) {
        auto& cache = imageCache();
        auto  it    = cache.find(path);
        if (it == cache.end()) {
            auto* s = cairo_image_surface_create_from_png(path.c_str());
            if (cairo_surface_status(s) != CAIRO_STATUS_SUCCESS) {
                cairo_surface_destroy(s); // unreadable: draw nothing (and try again next frame is pointless, remember the failure as a null surface)
                s = nullptr;
            }
            it = cache.emplace(path, s).first;
        }
        if (!it->second)
            return;
        const double iw = cairo_image_surface_get_width(it->second), ih = cairo_image_surface_get_height(it->second);
        if (iw <= 0 || ih <= 0)
            return;
        double sx = 1, sy = 1;
        if (w > 0 && h > 0)
            sx = w / iw, sy = h / ih;
        else if (w > 0)
            sx = sy = w / iw;
        else if (h > 0)
            sx = sy = h / ih;
        cairo_save(m_cr);
        cairo_translate(m_cr, x, y);
        cairo_scale(m_cr, sx, sy);
        cairo_set_source_surface(m_cr, it->second, 0, 0);
        cairo_paint(m_cr);
        cairo_restore(m_cr);
    }
}
