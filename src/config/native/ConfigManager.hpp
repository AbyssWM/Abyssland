#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <type_traits>

#include "../ConfigManager.hpp"
#include "ConfigCells.hpp"

namespace Config::Native {

    // Native (compiled-in) config backend. Owns one storage cell per option of Config::Values::CONFIG_VALUES.
    class CConfigManager : public Config::IConfigManager {
      public:
        CConfigManager();
        virtual ~CConfigManager() override = default;

        virtual void                            init() override;
        virtual void                            reload() override;
        virtual std::string                     verify() override;

        virtual int                             getDeviceInt(const std::string&, const std::string&, const std::string& fallback = "") override;
        virtual float                           getDeviceFloat(const std::string&, const std::string&, const std::string& fallback = "") override;
        virtual Vector2D                        getDeviceVec(const std::string&, const std::string&, const std::string& fallback = "") override;
        virtual std::string                     getDeviceString(const std::string&, const std::string&, const std::string& fallback = "") override;
        virtual bool                            deviceConfigExplicitlySet(const std::string&, const std::string&) override;
        virtual bool                            deviceConfigExists(const std::string&) override;

        virtual SConfigOptionReply              getConfigValue(const std::string&) override;

        virtual std::string                     getMainConfigPath() override;
        virtual std::string                     currentConfigPath() override;
        virtual std::string                     getConfigString() override;
        virtual const std::vector<std::string>& getConfigPaths() override;

        virtual bool                            configVerifPassed() override;
        virtual std::string                     getErrors() override;

        // ---- C++ setter API (used by the compiled-in user config) ----
        // Names accept both "general:gaps_in" and "general.gaps_in".
        // Returns false (and records an error that is shown on the error overlay) on unknown option, bad type or out-of-range value.
        template <typename T>
        bool set(std::string_view name, T&& value) {
            return setInput(name, toInput(std::forward<T>(value)));
        }

        // per-device override, e.g. setDevice("logitech-mouse", "sensitivity", -0.5). Device names get ' ' -> '-'.
        template <typename T>
        bool setDevice(std::string_view device, std::string_view field, T&& value) {
            return setDeviceInput(device, field, toInput(std::forward<T>(value)));
        }

        bool        setInput(std::string_view name, const ConfigInput& in);
        bool        setDeviceInput(std::string_view device, std::string_view field, const ConfigInput& in);
        // makes deviceConfigExists(device) true without overriding anything
        void        ensureDevice(std::string_view device);

        void        addError(std::string&& str);

        bool        isFirstLaunch() const;
        bool        isParsingConfig() const;

        static std::string normalizeName(std::string_view s);

      private:
        template <typename T>
        static ConfigInput toInput(T&& v) {
            using D = std::decay_t<T>;
            if constexpr (std::is_same_v<D, bool>)
                return ConfigInput{std::in_place_type<bool>, v};
            else if constexpr (std::is_integral_v<D>)
                return ConfigInput{std::in_place_type<int64_t>, sc<int64_t>(v)};
            else if constexpr (std::is_floating_point_v<D>)
                return ConfigInput{std::in_place_type<double>, sc<double>(v)};
            else if constexpr (std::is_convertible_v<D, std::string_view>)
                return ConfigInput{std::in_place_type<std::string>, std::string(std::string_view(v))};
            else
                return ConfigInput{std::forward<T>(v)};
        }

        void   postConfigReload();

        struct SDeviceConfig {
            SDeviceConfig();
            std::unordered_map<std::string, UP<ICell>> values;
        };

        ICell* findDeviceValue(const std::string& dev, const std::string& field);
        ICell* findValue(std::string_view name);

        std::unordered_map<std::string, UP<ICell>>   m_configValues;
        std::unordered_map<std::string, SDeviceConfig> m_deviceConfigs;
        std::vector<std::string>                     m_errors, m_configPaths;

        bool                                         m_lastConfigVerificationWasSuccessful = true;
        bool                                         m_isFirstLaunch                       = true;
        bool                                         m_manualCrashInitiated                = false;
        bool                                         m_isParsingConfig                     = false;
    };

    WP<CConfigManager> mgr();

    // Implemented in UserConfig.cpp: the compiled-in user config. Called from reload() after all state was reset to defaults.
    void applyUserConfig();
}
