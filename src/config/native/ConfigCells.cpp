#include "ConfigCells.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <sstream>

#include <abyssutils/string/String.hpp>

#include "../shared/parserUtils/ParserUtils.hpp"
#include "../values/types/IntValue.hpp"
#include "../values/types/FloatValue.hpp"
#include "../values/types/BoolValue.hpp"
#include "../values/types/StringValue.hpp"
#include "../values/types/ColorValue.hpp"
#include "../values/types/Vec2Value.hpp"
#include "../values/types/CssGapValue.hpp"
#include "../values/types/FontWeightValue.hpp"
#include "../values/types/GradientValue.hpp"

using namespace Config;
using namespace Config::Native;

Config::INTEGER ICell::asInt() const {
    return 0;
}

Config::FLOAT ICell::asFloat() const {
    return 0.F;
}

Config::VEC2 ICell::asVec2() const {
    return {};
}

Config::STRING ICell::asString() const {
    return {};
}

bool ICell::setByUser() const {
    return m_setByUser;
}

void ICell::resetSetByUser() {
    m_setByUser = false;
}

Supplementary::PropRefreshBits ICell::refreshBits() const {
    return m_refreshBits;
}

void ICell::setRefreshBits(Supplementary::PropRefreshBits bits) {
    m_refreshBits = bits;
}

const void* const* ICell::dataPtrPtr() {
    m_dataCache = data();
    return &m_dataCache;
}

// ---------------------------------------------------------------------------------------------

namespace {
    bool inIntegerRange(double value) {
        // INT64_MAX rounds up to 2^63 as a double, so the upper bound must be exclusive.
        constexpr auto MIN = sc<double>(std::numeric_limits<int64_t>::min());
        return std::isfinite(value) && value >= MIN && value < -MIN;
    }

    // numeric view of an input, if it has one
    std::optional<double> numeric(const ConfigInput& in) {
        if (auto p = std::get_if<bool>(&in))
            return *p ? 1.0 : 0.0;
        if (auto p = std::get_if<int64_t>(&in))
            return sc<double>(*p);
        if (auto p = std::get_if<double>(&in))
            return *p;
        return std::nullopt;
    }

    std::optional<int64_t> integral(const ConfigInput& in) {
        if (auto p = std::get_if<bool>(&in))
            return *p ? 1 : 0;
        if (auto p = std::get_if<int64_t>(&in))
            return *p;
        if (auto p = std::get_if<double>(&in); p && inIntegerRange(*p) && std::floor(*p) == *p)
            return sc<int64_t>(*p);
        return std::nullopt;
    }

    class CIntCell : public ICell {
      public:
        CIntCell(Config::INTEGER def, std::optional<Config::INTEGER> min, std::optional<Config::INTEGER> max, std::optional<std::unordered_map<std::string, Config::INTEGER>> map) :
            m_default(def), m_data(def), m_min(min), m_max(max), m_map(std::move(map)) {}

        const void* data() override {
            return &m_data;
        }
        const std::type_info* underlying() override {
            return &typeid(Config::INTEGER);
        }
        std::string toString() override {
            return std::to_string(m_data);
        }
        void reset() override {
            m_data = m_default;
        }
        Config::INTEGER asInt() const override {
            return m_data;
        }
        Config::FLOAT asFloat() const override {
            return sc<Config::FLOAT>(m_data);
        }

        SetResult set(const ConfigInput& in) override {
            if (auto s = std::get_if<std::string>(&in)) {
                if (!m_map)
                    return std::unexpected("integer type requires a bool or an integer");
                const auto it = m_map->find(*s);
                if (it == m_map->end()) {
                    std::string keys;
                    for (const auto& [k, _] : *m_map) {
                        if (!keys.empty())
                            keys += ", ";
                        keys += "\"" + k + "\"";
                    }
                    return std::unexpected(std::format("unknown string value \"{}\", acceptable values are: {}", *s, keys));
                }
                m_data      = it->second;
                m_setByUser = true;
                return {};
            }

            const auto V = integral(in);
            if (!V)
                return std::unexpected("integer type requires a bool or an integer");
            if (m_min && *V < *m_min)
                return std::unexpected(std::format("value {} is less than the minimum of {}", *V, *m_min));
            if (m_max && *V > *m_max)
                return std::unexpected(std::format("value {} is more than the maximum of {}", *V, *m_max));
            m_data      = *V;
            m_setByUser = true;
            return {};
        }

      private:
        Config::INTEGER                                                 m_default = 0, m_data = 0;
        std::optional<Config::INTEGER>                                  m_min, m_max;
        std::optional<std::unordered_map<std::string, Config::INTEGER>> m_map;
    };

    class CBoolCell : public ICell {
      public:
        CBoolCell(bool def) : m_default(def), m_data(def) {}

        const void* data() override {
            return &m_data;
        }
        const std::type_info* underlying() override {
            return &typeid(bool);
        }
        std::string toString() override {
            return m_data ? "true" : "false";
        }
        void reset() override {
            m_data = m_default;
        }
        Config::INTEGER asInt() const override {
            return m_data;
        }
        Config::FLOAT asFloat() const override {
            return m_data ? 1.F : 0.F;
        }

        SetResult set(const ConfigInput& in) override {
            const auto V = integral(in);
            if (!V || (*V != 0 && *V != 1))
                return std::unexpected("bool type requires a bool (or 0 / 1)");
            m_data      = *V;
            m_setByUser = true;
            return {};
        }

      private:
        bool m_default = false, m_data = false;
    };

    class CFloatCell : public ICell {
      public:
        CFloatCell(Config::FLOAT def, std::optional<Config::FLOAT> min, std::optional<Config::FLOAT> max) : m_default(def), m_data(def), m_min(min), m_max(max) {}

        const void* data() override {
            return &m_data;
        }
        const std::type_info* underlying() override {
            return &typeid(Config::FLOAT);
        }
        std::string toString() override {
            return std::format("{}", m_data);
        }
        void reset() override {
            m_data = m_default;
        }
        Config::FLOAT asFloat() const override {
            return m_data;
        }
        Config::INTEGER asInt() const override {
            // Match ICell's fallback when this value has no representable integer view.
            if (!inIntegerRange(m_data))
                return 0;
            return sc<Config::INTEGER>(m_data);
        }

        SetResult set(const ConfigInput& in) override {
            const auto V = numeric(in);
            if (!V)
                return std::unexpected("float type requires a number");
            if (!std::isfinite(*V) || *V < std::numeric_limits<Config::FLOAT>::lowest() || *V > std::numeric_limits<Config::FLOAT>::max())
                return std::unexpected("float type requires a finite number within the float range");
            if (m_min && *V < *m_min)
                return std::unexpected(std::format("value {:.2f} is less than the minimum of {:.2f}", *V, *m_min));
            if (m_max && *V > *m_max)
                return std::unexpected(std::format("value {:.2f} is more than the maximum of {:.2f}", *V, *m_max));
            m_data      = *V;
            m_setByUser = true;
            return {};
        }

      private:
        Config::FLOAT                m_default = 0.F, m_data = 0.F;
        std::optional<Config::FLOAT> m_min, m_max;
    };

    class CStringCell : public ICell {
      public:
        CStringCell(Config::STRING def, std::function<std::expected<void, std::string>(const std::string&)> validator) : m_default(def), m_data(def), m_validator(std::move(validator)) {}

        const void* data() override {
            return &m_data;
        }
        const std::type_info* underlying() override {
            return &typeid(Config::STRING);
        }
        std::string toString() override {
            return m_data;
        }
        void reset() override {
            m_data = m_default;
        }
        Config::STRING asString() const override {
            return m_data;
        }

        SetResult set(const ConfigInput& in) override {
            const auto S = std::get_if<std::string>(&in);
            if (!S)
                return std::unexpected("string type requires a string");
            if (m_validator) {
                if (auto res = m_validator(*S); !res)
                    return std::unexpected(res.error());
            }
            m_data      = *S;
            m_setByUser = true;
            return {};
        }

      private:
        Config::STRING                                                      m_default = "[[EMPTY]]", m_data = "[[EMPTY]]";
        std::function<std::expected<void, std::string>(const std::string&)> m_validator;
    };

    class CColorCell : public ICell {
      public:
        CColorCell(Config::INTEGER def) : m_default(def), m_data(def) {}

        const void* data() override {
            return &m_data;
        }
        const std::type_info* underlying() override {
            return &typeid(Config::INTEGER);
        }
        std::string toString() override {
            return std::format("0x{:08X}", sc<uint32_t>(m_data));
        }
        void reset() override {
            m_data = m_default;
        }
        Config::INTEGER asInt() const override {
            return m_data;
        }

        SetResult set(const ConfigInput& in) override {
            if (auto s = std::get_if<std::string>(&in)) {
                const auto C = ParserUtils::parseColor(*s);
                if (!C)
                    return std::unexpected(std::format("invalid color \"{}\"", *s));
                m_data = CAbyssColor(sc<uint64_t>(*C)).getAsHex();
            } else if (auto c = std::get_if<CAbyssColor>(&in))
                m_data = c->getAsHex();
            else if (auto i = std::get_if<int64_t>(&in))
                m_data = CAbyssColor(sc<uint64_t>(*i)).getAsHex();
            else
                return std::unexpected("color type requires a color string, a color or an integer");
            m_setByUser = true;
            return {};
        }

      private:
        Config::INTEGER m_default = 0, m_data = 0;
    };

    class CVec2Cell : public ICell {
      public:
        CVec2Cell(Config::VEC2 def, std::function<std::expected<void, std::string>(const Config::VEC2&)> validator) : m_default(def), m_data(def), m_validator(std::move(validator)) {}

        const void* data() override {
            return &m_data;
        }
        const std::type_info* underlying() override {
            return &typeid(Config::VEC2);
        }
        std::string toString() override {
            return std::format("{} {}", m_data.x, m_data.y);
        }
        void reset() override {
            m_data = m_default;
        }
        Config::VEC2 asVec2() const override {
            return m_data;
        }

        SetResult set(const ConfigInput& in) override {
            Config::VEC2 vec = {};

            if (auto v = std::get_if<Config::VEC2>(&in))
                vec = *v;
            else if (auto s = std::get_if<std::string>(&in)) {
                std::string input = *s;
                std::ranges::replace(input, ',', ' ');
                std::istringstream is(input);
                double             x = 0, y = 0;
                std::string        tail;
                if (!(is >> x >> y) || (is >> tail))
                    return std::unexpected("vec2 string requires exactly 2 numbers (e.g. \"1 1\")");
                vec = {sc<float>(x), sc<float>(y)};
            } else
                return std::unexpected("vec2 type requires a vec2 or a string");

            if (m_validator) {
                if (auto res = m_validator(vec); !res)
                    return std::unexpected(res.error());
            }
            m_data      = vec;
            m_setByUser = true;
            return {};
        }

      private:
        Config::VEC2                                                         m_default = {}, m_data = {};
        std::function<std::expected<void, std::string>(const Config::VEC2&)> m_validator;
    };

    class CCssGapCell : public ICell {
      public:
        CCssGapCell(Config::INTEGER def, std::optional<int64_t> min, std::optional<int64_t> max) : m_default(def), m_data(def), m_min(min), m_max(max) {}

        const void* data() override {
            return dc<const IComplexConfigValue*>(&m_data);
        }
        const std::type_info* underlying() override {
            return &typeid(Config::CCssGapData);
        }
        std::string toString() override {
            return m_data.toString();
        }
        void reset() override {
            m_data = CCssGapData(m_default);
        }
        Config::INTEGER asInt() const override {
            return m_data.m_top;
        }

        SetResult set(const ConfigInput& in) override {
            CCssGapData d;
            if (auto g = std::get_if<CCssGapData>(&in))
                d = *g;
            else if (auto i = integral(in))
                d = CCssGapData(*i);
            else
                return std::unexpected("css_gap type requires an integer or a CCssGapData");

            for (const auto& [name, v] : {std::pair<const char*, int64_t>{"top", d.m_top}, {"right", d.m_right}, {"bottom", d.m_bottom}, {"left", d.m_left}}) {
                if (m_min && v < *m_min)
                    return std::unexpected(std::format("gap \"{}\" value {} is less than the minimum of {}", name, v, *m_min));
                if (m_max && v > *m_max)
                    return std::unexpected(std::format("gap \"{}\" value {} is more than the maximum of {}", name, v, *m_max));
            }
            m_data      = d;
            m_setByUser = true;
            return {};
        }

      private:
        Config::INTEGER       m_default = 0;
        Config::CCssGapData   m_data;
        std::optional<int64_t> m_min, m_max;
    };

    class CFontWeightCell : public ICell {
      public:
        CFontWeightCell(Config::INTEGER def) : m_default(def), m_data(def) {}

        const void* data() override {
            return dc<const IComplexConfigValue*>(&m_data);
        }
        const std::type_info* underlying() override {
            return &typeid(Config::CFontWeightConfigValueData);
        }
        std::string toString() override {
            return m_data.toString();
        }
        void reset() override {
            m_data = CFontWeightConfigValueData(m_default);
        }
        Config::INTEGER asInt() const override {
            return m_data.m_value;
        }

        SetResult set(const ConfigInput& in) override {
            if (auto w = std::get_if<CFontWeightConfigValueData>(&in)) {
                m_data      = *w;
                m_setByUser = true;
                return {};
            }

            if (auto i = integral(in)) {
                if (*i < 0)
                    return std::unexpected(std::format("font weight {} must not be negative", *i));
                m_data.m_value = *i;
                m_setByUser    = true;
                return {};
            }

            if (auto s = std::get_if<std::string>(&in)) {
                std::string lc = *s;
                std::ranges::transform(*s, lc.begin(), ::tolower);
                const auto it = CFontWeightConfigValueData::WEIGHTS.find(lc);
                if (it != CFontWeightConfigValueData::WEIGHTS.end()) {
                    m_data.m_value = it->second;
                    m_setByUser    = true;
                    return {};
                }
                if (Abyssutils::String::isNumber(*s))
                    return std::unexpected(std::format("font weight \"{}\" was not found. Did you mean to put in an integer (without quotes?)", *s));
                return std::unexpected(std::format("font weight \"{}\" was not found", *s));
            }

            return std::unexpected("font weight type requires a number or a weight name string");
        }

      private:
        Config::INTEGER                    m_default = 400;
        Config::CFontWeightConfigValueData m_data;
    };

    class CGradientCell : public ICell {
      public:
        CGradientCell(CAbyssColor def) : m_default(def), m_data(def) {}

        const void* data() override {
            return dc<const IComplexConfigValue*>(&m_data);
        }
        const std::type_info* underlying() override {
            return &typeid(Config::CGradientValueData);
        }
        std::string toString() override {
            return m_data.toString();
        }
        void reset() override {
            m_data = CGradientValueData(m_default);
        }

        SetResult set(const ConfigInput& in) override {
            if (auto g = std::get_if<CGradientValueData>(&in)) {
                if (g->m_colors.empty())
                    return std::unexpected("gradient needs at least one color");
                if (g->m_colors.size() > 10)
                    return std::unexpected("gradient supports at most 10 colors");
                m_data = *g;
                m_data.updateColorsOk();
            } else if (auto c = std::get_if<CAbyssColor>(&in))
                m_data.reset(*c);
            else if (auto s = std::get_if<std::string>(&in)) {
                const auto C = ParserUtils::parseColor(*s);
                if (!C)
                    return std::unexpected(std::format("invalid color \"{}\"", *s));
                m_data.reset(CAbyssColor(sc<uint64_t>(*C)));
            } else
                return std::unexpected("gradient type requires a gradient, a color or a color string");
            m_setByUser = true;
            return {};
        }

      private:
        CAbyssColor                m_default;
        Config::CGradientValueData m_data;
    };
}

UP<ICell> Native::makeBoolCell(bool def) {
    return makeUnique<CBoolCell>(def);
}

UP<ICell> Native::makeStringCell(Config::STRING def) {
    return makeUnique<CStringCell>(def, nullptr);
}

UP<ICell> Native::makeIntCell(Config::INTEGER def, Config::INTEGER min, Config::INTEGER max) {
    return makeUnique<CIntCell>(def, min, max, std::nullopt);
}

UP<ICell> Native::makeFloatCell(Config::FLOAT def, Config::FLOAT min, Config::FLOAT max) {
    return makeUnique<CFloatCell>(def, min, max);
}

UP<ICell> Native::cellFromValue(SP<Config::Values::IValue> v) {
    UP<ICell> cell;

    if (auto p = dc<Config::Values::CIntValue*>(v.get()))
        cell = makeUnique<CIntCell>(p->defaultVal(), p->m_min, p->m_max, p->m_map);
    else if (auto p = dc<Config::Values::CFloatValue*>(v.get()))
        cell = makeUnique<CFloatCell>(p->defaultVal(), p->m_min, p->m_max);
    else if (auto p = dc<Config::Values::CBoolValue*>(v.get()))
        cell = makeUnique<CBoolCell>(p->defaultVal());
    else if (auto p = dc<Config::Values::CStringValue*>(v.get()))
        cell = makeUnique<CStringCell>(p->defaultVal(), p->validator());
    else if (auto p = dc<Config::Values::CColorValue*>(v.get()))
        cell = makeUnique<CColorCell>(p->defaultVal());
    else if (auto p = dc<Config::Values::CVec2Value*>(v.get()))
        cell = makeUnique<CVec2Cell>(p->defaultVal(), p->validator());
    else if (auto p = dc<Config::Values::CCssGapValue*>(v.get()))
        cell = makeUnique<CCssGapCell>(p->defaultVal().m_top, p->m_min, p->m_max);
    else if (auto p = dc<Config::Values::CFontWeightValue*>(v.get()))
        cell = makeUnique<CFontWeightCell>(p->defaultVal().m_value);
    else if (auto p = dc<Config::Values::CGradientValue*>(v.get()))
        cell = makeUnique<CGradientCell>(p->defaultVal().m_colors.empty() ? CAbyssColor{} : p->defaultVal().m_colors.front());

    if (cell)
        cell->setRefreshBits(v->refreshBits());

    return cell;
}
