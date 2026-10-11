#pragma once

// Storage cells for the native config. One cell per option of Config::Values::CONFIG_VALUES.
// A cell owns the value at a stable address, so CConfigValue<> readers can keep raw pointers.

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <variant>

#include "../shared/Types.hpp"
#include "../shared/complex/ComplexDataTypes.hpp"
#include "../supplementary/propRefresher/PropRefresher.hpp"
#include "../../helpers/memory/Memory.hpp"
#include "../../helpers/Color.hpp"
#include "../values/types/IValue.hpp"

namespace Config::Native {

    // everything a cell can be set from. Cells coerce where it is lossless (bool->int, int->float, string->color...)
    using ConfigInput = std::variant<bool, int64_t, double, std::string, Config::VEC2, CAbyssColor, Config::CGradientValueData, Config::CCssGapData, Config::CFontWeightConfigValueData>;

    using SetResult = std::expected<void, std::string>;

    class ICell {
      public:
        virtual ~ICell() = default;

        // never moves for the lifetime of the cell
        virtual const void*                    data()       = 0;
        virtual const std::type_info*          underlying() = 0;
        virtual std::string                    toString()   = 0;
        virtual void                           reset()      = 0;
        virtual SetResult                      set(const ConfigInput& in) = 0;

        virtual Config::INTEGER                asInt() const;
        virtual Config::FLOAT                  asFloat() const;
        virtual Config::VEC2                   asVec2() const;
        virtual Config::STRING                 asString() const;

        bool                                   setByUser() const;
        void                                   resetSetByUser();
        Supplementary::PropRefreshBits         refreshBits() const;
        void                                   setRefreshBits(Supplementary::PropRefreshBits bits);

        // pointer-to-pointer handed out by getConfigValue(), what CConfigValue<> derefs
        const void* const*                     dataPtrPtr();

      protected:
        bool                           m_setByUser = false;

      private:
        Supplementary::PropRefreshBits m_refreshBits = 0;
        const void*                    m_dataCache   = nullptr;
    };

    // creates a cell from an entry of the central option table
    UP<ICell> cellFromValue(SP<Config::Values::IValue> v);

    // a bool cell, used for the per-device "keybinds" toggle
    UP<ICell> makeBoolCell(bool def);
}
