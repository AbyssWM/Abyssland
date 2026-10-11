#include "tests.hpp"
#include "../../shared.hpp"
#include "../../abyssctlCompat.hpp"
#include <abyssutils/os/Process.hpp>
#include <abyssutils/memory/WeakPtr.hpp>
#include <abyssutils/utils/ScopeGuard.hpp>
#include "../shared.hpp"

using namespace Abyssutils::OS;
using namespace Abyssutils::Memory;
using namespace Abyssutils::Utils;

#define UP CUniquePointer
#define SP CSharedPointer

TEST_CASE(crossMonitorFullscreenFocus) {
    // Create a destination monitor to the right of the default one and pin the
    // destination workspace to it
    OK(getFromSocket("/eval hl.monitor({ output = 'ABYSSTEST-2', mode = '1920x1080@60', position = 'auto-right', scale = '1' })"));
    OK(getFromSocket("/output create headless ABYSSTEST-2"));
    OK(getFromSocket("/eval hl.workspace_rule({ workspace = 'name:workspace2', monitor = 'ABYSSTEST-2' })"));

    // 1 window on the left monitor
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:workspace1' })"));
    auto src = Tests::spawnKitty("1A");
    if (!src)
        FAIL_TEST("Could not spawn kitty");
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:1A' })"));
    const auto MON_SRC_ID = Tests::getAttribute(getFromSocket("/activewindow"), "monitor");

    // 2 windows on the right monitor
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:workspace2' })"));
    auto dstA = Tests::spawnKitty("2A");
    auto dstB = Tests::spawnKitty("2B");
    if (!dstA || !dstB)
        FAIL_TEST("Could not spawn destination kittys");
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:2B' })"));
    const auto MON_DST_ID = Tests::getAttribute(getFromSocket("/activewindow"), "monitor");

    // Sanity check: the two windows really do live on different monitors
    ASSERT_NOT(MON_SRC_ID, MON_DST_ID);

    // float and fullscreen 2B
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:2B' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'toggle' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'toggle' })"));

    // focus 1A and focus to the right
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:1A' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = \"right\" })"));

    const auto active = getFromSocket("/activewindow");
    EXPECT_CONTAINS(active, "class: 2B");

    Tests::killAllWindows();
    OK(getFromSocket("/output remove ABYSSTEST-2"));
}

TEST_CASE(crossMonitorEmptyWorkspaceUnfocusesWindow) {
    getFromSocket("/output remove ABYSSTEST-UNFOCUS");
    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', mode = '1920x1080@60', position = '0x0', scale = '1' })"));
    OK(getFromSocket("/eval hl.monitor({ output = 'ABYSSTEST-UNFOCUS', mode = '1920x1080@60', position = '1920x0', scale = '1' })"));
    OK(getFromSocket("/output create headless ABYSSTEST-UNFOCUS"));

    CScopeGuard guard = {[&]() {
        Tests::killAllWindows();
        OK(getFromSocket("/output remove ABYSSTEST-UNFOCUS"));
    }};

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    ASSERT(Tests::windowCount(), 0);

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'ABYSSTEST-UNFOCUS' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '2' })"));

    auto kittyProc = Tests::spawnKitty("cross_monitor_ws2");
    if (!kittyProc)
        FAIL_TEST("Could not spawn kitty");

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: cross_monitor_ws2");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        ASSERT_CONTAINS(str, "workspace ID 1 ");
    }

    ASSERT(getFromSocket("/activewindow"), "Invalid");
}
