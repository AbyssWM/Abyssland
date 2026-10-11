#include "tests.hpp"
#include "../../shared.hpp"
#include "../../abyssctlCompat.hpp"

TEST_CASE(luaRequire) {
    constexpr auto EXPECTED = "absolute:relative:a:b";

    EXPECT(getFromSocket("/repl return _G.abysstester_lua_require_result"), EXPECTED);

    OK(getFromSocket("/reload"));
    EXPECT(getFromSocket("/repl return _G.abysstester_lua_require_result"), EXPECTED);
}
