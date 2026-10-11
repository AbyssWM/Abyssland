#include <gtest/gtest.h>

#include "../../src/xwayland/CursorResources.hpp"

TEST(CursorResources, AddsMissingResources) {
    EXPECT_EQ(CursorResources::merge("", "Sweet-cursors", 32), "Xcursor.theme: Sweet-cursors\nXcursor.size: 32\n");
}

TEST(CursorResources, PreservesUnrelatedResourcesAndComments) {
    const std::string ORIGINAL = "! Xcursor.theme: comment\n# heading\nXft.dpi: 144\napp*foreground: #c0ffee\nXcursor.theme.extra: untouched\nXcursor*theme: wildcard\n";
    EXPECT_EQ(CursorResources::merge(ORIGINAL, "default", 24), ORIGINAL + "Xcursor.theme: default\nXcursor.size: 24\n");
    EXPECT_EQ(CursorResources::merge("Xft.dpi: 144", "default", 24), "Xft.dpi: 144\nXcursor.theme: default\nXcursor.size: 24\n");
}

TEST(CursorResources, RemovesDuplicatesAndWhitespace) {
    const auto MERGED = CursorResources::merge("Xcursor.theme: old\n \tXcursor.size \t: 99\nXft.dpi: 144\nXcursor.theme: newer\nXcursor.size: 16", "new", 32);
    ASSERT_TRUE(MERGED);
    EXPECT_EQ(*MERGED, "Xft.dpi: 144\nXcursor.theme: new\nXcursor.size: 32\n");
    EXPECT_EQ(CursorResources::merge(*MERGED, "new", 32), MERGED);
}

TEST(CursorResources, HandlesLogicalLineContinuations) {
    EXPECT_EQ(CursorResources::merge("Xcursor.theme: old\\\n continuation\nother.value: keep\\\nXcursor.size: continued value\nXcursor.size: 16\n", "new", 24),
              "other.value: keep\\\nXcursor.size: continued value\nXcursor.theme: new\nXcursor.size: 24\n");
    EXPECT_EQ(CursorResources::merge("other.value: double\\\\\nXcursor.size: 16\n", "new", 24), "other.value: double\\\\\nXcursor.theme: new\nXcursor.size: 24\n");
    EXPECT_EQ(CursorResources::merge("other.value: dangling\\", "new", 24), "other.value: dangling\\\n\nXcursor.theme: new\nXcursor.size: 24\n");
    EXPECT_EQ(CursorResources::merge("other.value: dangling\\\n", "new", 24), "other.value: dangling\\\n\nXcursor.theme: new\nXcursor.size: 24\n");
    EXPECT_EQ(CursorResources::merge("! comment\\\nXcursor.theme: old\n", "new", 24), "! comment\\\n\nXcursor.theme: new\nXcursor.size: 24\n");
    EXPECT_EQ(CursorResources::merge("# comment\\\nXcursor.size: 16\n", "new", 24), "# comment\\\n\nXcursor.theme: new\nXcursor.size: 24\n");
}

TEST(CursorResources, EscapesThemeValues) {
    EXPECT_EQ(CursorResources::merge("", " a\\b:!# c ", 24), "Xcursor.theme: \\040a\\\\b:!#\\040c\\040\nXcursor.size: 24\n");
    EXPECT_EQ(CursorResources::merge("", "café", 24), "Xcursor.theme: café\nXcursor.size: 24\n");
}

TEST(CursorResources, RejectsInvalidValues) {
    for (const auto THEME : {"", "bad\ntheme", "bad\rtheme", "bad\ttheme", "bad\x7f"})
        EXPECT_FALSE(CursorResources::merge("", THEME, 24));
    EXPECT_FALSE(CursorResources::merge("", std::string_view{"bad\0theme", 9}, 24));
    EXPECT_FALSE(CursorResources::merge("", "default", 0));
    EXPECT_FALSE(CursorResources::merge("", "default", -1));
    EXPECT_FALSE(CursorResources::merge(std::string_view{"Xft.dpi: 144\0other: value", 25}, "default", 24));
}

TEST(CursorResources, PreservesOptionalTerminator) {
    const std::string ORIGINAL{"Xft.dpi: 144\0", 13};
    std::string       expected = "Xft.dpi: 144\nXcursor.theme: default\nXcursor.size: 24\n";
    expected += '\0';
    EXPECT_EQ(CursorResources::merge(ORIGINAL, "default", 24), expected);
}
