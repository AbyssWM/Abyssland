#include <config/native/ConfigCells.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

using namespace Config::Native;

template <typename T>
static void expectRejected(ICell& cell, const ConfigInput& input, const ConfigInput& valid) {
    for (const bool setByUser : {false, true}) {
        ASSERT_TRUE(cell.set(valid).has_value());
        if (!setByUser)
            cell.resetSetByUser();

        const auto before = *static_cast<const T*>(cell.data());
        const auto data   = cell.data();
        EXPECT_FALSE(cell.set(input).has_value());
        EXPECT_EQ(*static_cast<const T*>(cell.data()), before);
        EXPECT_EQ(cell.data(), data);
        EXPECT_EQ(cell.setByUser(), setByUser);
    }
}

TEST(NativeConfigCells, IntegerAcceptsExactBoundaries) {
    const auto min  = std::numeric_limits<int64_t>::min();
    const auto max  = std::numeric_limits<int64_t>::max();
    auto       cell = makeIntCell(7, min, max);

    for (const auto value : {min, max, int64_t{0}}) {
        ASSERT_TRUE(cell->set(value).has_value());
        EXPECT_EQ(cell->asInt(), value);
    }
    ASSERT_TRUE(cell->set(-0x1p63).has_value());
    EXPECT_EQ(cell->asInt(), min);
    const auto belowMax = std::nextafter(0x1p63, 0.0);
    ASSERT_TRUE(cell->set(belowMax).has_value());
    EXPECT_EQ(cell->asInt(), static_cast<int64_t>(belowMax));
    ASSERT_TRUE(cell->set(true).has_value());
    EXPECT_EQ(cell->asInt(), 1);
}

TEST(NativeConfigCells, IntegerRejectsNonIntegralOrUnrepresentableInputsWithoutMutation) {
    auto cell = makeIntCell(7, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max());
    for (const double value : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), 0x1p63,
                               std::nextafter(-0x1p63, -std::numeric_limits<double>::infinity()), 1.5, -1.5})
        expectRejected<Config::INTEGER>(*cell, value, int64_t{7});

    auto bounded = makeIntCell(3, 1, 5);
    expectRejected<Config::INTEGER>(*bounded, int64_t{0}, int64_t{3});
    expectRejected<Config::INTEGER>(*bounded, 6.0, int64_t{3});
}

TEST(NativeConfigCells, BoolKeepsZeroAndOneCoercions) {
    auto cell = makeBoolCell(false);
    for (const ConfigInput value : {ConfigInput{false}, ConfigInput{int64_t{0}}, ConfigInput{0.0}}) {
        ASSERT_TRUE(cell->set(value).has_value());
        EXPECT_EQ(cell->asInt(), 0);
    }
    for (const ConfigInput value : {ConfigInput{true}, ConfigInput{int64_t{1}}, ConfigInput{1.0}}) {
        ASSERT_TRUE(cell->set(value).has_value());
        EXPECT_EQ(cell->asInt(), 1);
    }
    for (const double value :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), -0x1p63, 0x1p63, 0.5, 2.0})
        expectRejected<bool>(*cell, value, true);
}

TEST(NativeConfigCells, FloatRejectsNonFiniteAndOverflowWithoutMutation) {
    const auto max = std::numeric_limits<float>::max();
    // Infinite option bounds must not bypass the storage type's own range checks.
    auto cell = makeFloatCell(0.25F, -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity());
    for (const double value :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::max(),
          -std::numeric_limits<double>::max(), std::nextafter(static_cast<double>(max), std::numeric_limits<double>::infinity()),
          std::nextafter(-static_cast<double>(max), -std::numeric_limits<double>::infinity())})
        expectRejected<Config::FLOAT>(*cell, value, 0.25);

    auto bounded = makeFloatCell(1.F, 0.F, 2.F);
    expectRejected<Config::FLOAT>(*bounded, -0.5, 1.0);
    expectRejected<Config::FLOAT>(*bounded, 2.5, 1.0);
}

TEST(NativeConfigCells, FloatAcceptsFiniteFloatRangeAndExistingCoercions) {
    const auto max  = std::numeric_limits<float>::max();
    auto       cell = makeFloatCell(0.F, -max, max);
    for (const double value : {static_cast<double>(max), -static_cast<double>(max), 1.5, -0.25, 0.0, static_cast<double>(std::numeric_limits<float>::denorm_min())}) {
        ASSERT_TRUE(cell->set(value).has_value());
        EXPECT_EQ(cell->asFloat(), static_cast<float>(value));
        EXPECT_TRUE(cell->setByUser());
    }
    ASSERT_TRUE(cell->set(true).has_value());
    EXPECT_EQ(cell->asFloat(), 1.F);
    ASSERT_TRUE(cell->set(std::numeric_limits<int64_t>::max()).has_value());
    EXPECT_EQ(cell->asFloat(), static_cast<float>(std::numeric_limits<int64_t>::max()));
}

TEST(NativeConfigCells, FloatIntegerViewTruncatesOrReturnsGetterFallback) {
    const auto max  = std::numeric_limits<float>::max();
    auto       cell = makeFloatCell(0.F, -max, max);
    for (const double value : {1.75, -1.75, -0x1p63, static_cast<double>(std::nextafter(0x1p63F, 0.F))}) {
        ASSERT_TRUE(cell->set(value).has_value());
        EXPECT_EQ(cell->asInt(), static_cast<int64_t>(value));
    }
    for (const double value : {0x1p63, std::nextafter(0x1p63, 0.0), static_cast<double>(max), -static_cast<double>(max),
                               static_cast<double>(std::nextafter(-0x1p63F, -std::numeric_limits<float>::infinity()))}) {
        ASSERT_TRUE(cell->set(value).has_value());
        EXPECT_EQ(cell->asInt(), 0);
    }
    for (const float value : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()}) {
        auto invalidDefault = makeFloatCell(value, -max, max);
        EXPECT_EQ(invalidDefault->asInt(), 0);
    }
}
