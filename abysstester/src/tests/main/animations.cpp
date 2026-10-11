#include "../../abyssctlCompat.hpp"
#include "../../Log.hpp"
#include "../../shared.hpp"
#include "tests.hpp"

using namespace Abyssutils::OS;
using namespace Abyssutils::Memory;

TEST_CASE(animationsTrivial) {
    auto str = getFromSocket("/animations");
    NLog::yellow("Testing bezier curve output from `abyssctl animations`");
    ASSERT_CONTAINS(str, std::format("beziers:\n\n\tname: quick\n\t\tX0: 0.15\n\t\tY0: 0.00\n\t\tX1: 0.10\n\t\tY1: 1.00"));
}
