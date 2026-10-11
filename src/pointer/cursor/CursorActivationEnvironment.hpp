#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <gio/gio.h>

namespace Pointer::Cursor {
    struct SActivationEnvironment {
        std::string nativeTheme;
        std::string clientTheme;
        int         size = 24;
    };

    // Owns a connection to the explicit session bus; never autolaunches a bus or a user manager.
    class CActivationEnvironment {
      public:
        explicit CActivationEnvironment(std::string address);
        ~CActivationEnvironment();

        void                     update(SActivationEnvironment environment);
        std::vector<std::string> takeErrors();
        bool                     busy();

      private:
        void                                                     run();
        std::vector<std::string>                                 exportEnvironment(const SActivationEnvironment& environment);

        std::string                                              m_address;
        std::unique_ptr<GCancellable, decltype(&g_object_unref)> m_cancel;
        std::mutex                                               m_mutex;
        std::condition_variable                                  m_condition;
        std::optional<SActivationEnvironment>                    m_pending;
        std::vector<std::string>                                 m_errors;
        bool                                                     m_stopping  = false;
        bool                                                     m_exporting = false;
        std::thread                                              m_worker;
    };
}
