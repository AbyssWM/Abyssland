#include <abyss/actions.hpp>
#include <gtest/gtest.h>

TEST(NativeAction, DefersCallbackUntilRun) {
    int           calls = 0;
    abyss::Action action{[&calls] {
        ++calls;
        return abyss::ok();
    }};
    const auto    copied = action;

    EXPECT_EQ(calls, 0);
    EXPECT_TRUE(abyss::run(copied).success);
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(action().success);
    EXPECT_EQ(calls, 2);
}

TEST(NativeAction, PreservesCallbackError) {
    const abyss::Action action{[] { return abyss::error("command failed"); }};
    const auto          result = abyss::run(action);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, "command failed");
}

TEST(NativeAction, SupportsVoidCallback) {
    bool                called = false;
    const abyss::Action action{[&called] { called = true; }};

    EXPECT_FALSE(called);
    EXPECT_TRUE(abyss::run(action).success);
    EXPECT_TRUE(called);
}
