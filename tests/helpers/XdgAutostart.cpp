#include <helpers/XdgAutostart.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <unistd.h>

TEST(XdgAutostart, parseReadsTheDesktopEntryGroupOnly) {
    const auto ENTRY = XdgAutostart::parse("[Desktop Entry]\n"
                                           "Type=Application\n"
                                           "Name=Applet\n"
                                           "Name[de]=Applet\n"
                                           "Exec=nm-applet --indicator\n"
                                           "TryExec=nm-applet\n"
                                           "OnlyShowIn=Abyssland;Hyprland;\n"
                                           "NotShowIn = GNOME ; KDE;\n"
                                           "\n"
                                           "[Desktop Action quit]\n"
                                           "Exec=pkill nm-applet\n");
    ASSERT_TRUE(ENTRY.has_value());
    EXPECT_EQ(ENTRY->exec, "nm-applet --indicator");
    EXPECT_EQ(ENTRY->tryExec, "nm-applet");
    EXPECT_EQ(ENTRY->onlyShowIn, (std::vector<std::string>{"Abyssland", "Hyprland"}));
    EXPECT_EQ(ENTRY->notShowIn, (std::vector<std::string>{"GNOME", "KDE"}));
    EXPECT_TRUE(ENTRY->application);
    EXPECT_FALSE(ENTRY->hidden);
}

TEST(XdgAutostart, parseWithoutDesktopEntryGroup) {
    EXPECT_FALSE(XdgAutostart::parse("# nothing here\nExec=foo\n").has_value());
}

TEST(XdgAutostart, wantedHonorsHiddenDisabledAndType) {
    auto entry = XdgAutostart::parse("[Desktop Entry]\nType=Application\nExec=foo\n").value();
    EXPECT_TRUE(XdgAutostart::wanted(entry));

    EXPECT_FALSE(XdgAutostart::wanted(XdgAutostart::parse("[Desktop Entry]\nExec=foo\nHidden=true\n").value()));
    EXPECT_FALSE(XdgAutostart::wanted(XdgAutostart::parse("[Desktop Entry]\nExec=foo\nX-GNOME-Autostart-enabled=false\n").value()));
    EXPECT_FALSE(XdgAutostart::wanted(XdgAutostart::parse("[Desktop Entry]\nType=Link\nExec=foo\n").value()));
    EXPECT_FALSE(XdgAutostart::wanted(XdgAutostart::parse("[Desktop Entry]\nType=Application\n").value()));
    EXPECT_FALSE(XdgAutostart::wanted(XdgAutostart::parse("[Desktop Entry]\nExec=%U\n").value()));
}

TEST(XdgAutostart, shownIn) {
    const auto ANY      = XdgAutostart::parse("[Desktop Entry]\nExec=a\n").value();
    const auto ONLYKDE  = XdgAutostart::parse("[Desktop Entry]\nExec=a\nOnlyShowIn=KDE;\n").value();
    const auto NOTGNOME = XdgAutostart::parse("[Desktop Entry]\nExec=a\nNotShowIn=GNOME;\n").value();

    EXPECT_TRUE(XdgAutostart::shownIn(ANY, "Abyssland"));
    EXPECT_TRUE(XdgAutostart::shownIn(ANY, ""));
    EXPECT_FALSE(XdgAutostart::shownIn(ONLYKDE, "Abyssland"));
    EXPECT_TRUE(XdgAutostart::shownIn(ONLYKDE, "Abyssland:KDE"));
    EXPECT_TRUE(XdgAutostart::shownIn(NOTGNOME, "Abyssland"));
    EXPECT_FALSE(XdgAutostart::shownIn(NOTGNOME, "GNOME"));
}

TEST(XdgAutostart, stripFieldCodes) {
    EXPECT_EQ(XdgAutostart::stripFieldCodes("app %U"), "app");
    EXPECT_EQ(XdgAutostart::stripFieldCodes("app --x %f --y"), "app --x  --y");
    EXPECT_EQ(XdgAutostart::stripFieldCodes("printf 100%%"), "printf 100%");
    EXPECT_EQ(XdgAutostart::stripFieldCodes("app %"), "app");
}

TEST(XdgAutostart, program) {
    EXPECT_EQ(XdgAutostart::program("/usr/bin/pipewire -c pipewire-pulse.conf"), "pipewire");
    EXPECT_EQ(XdgAutostart::program("  wireplumber"), "wireplumber");
    EXPECT_EQ(XdgAutostart::program("\"/opt/my app/run\" --x"), "run");
    EXPECT_EQ(XdgAutostart::program("'/usr/bin/foo'"), "foo");
    EXPECT_EQ(XdgAutostart::program(""), "");
}

TEST(XdgAutostart, collectLetsEarlierDirectoriesWin) {
    namespace fs   = std::filesystem;
    const auto DIR = fs::temp_directory_path() / ("abyss-autostart-test-" + std::to_string(getpid()));
    fs::create_directories(DIR / "user");
    fs::create_directories(DIR / "system");

    std::ofstream{DIR / "user" / "applet.desktop"} << "[Desktop Entry]\nExec=user-applet\nHidden=true\n";
    std::ofstream{DIR / "system" / "applet.desktop"} << "[Desktop Entry]\nExec=system-applet\n";
    std::ofstream{DIR / "system" / "other.desktop"} << "[Desktop Entry]\nExec=other\n";
    std::ofstream{DIR / "system" / "notes.txt"} << "[Desktop Entry]\nExec=ignored\n";

    const auto ENTRIES = XdgAutostart::collect({DIR / "user", DIR / "system", DIR / "missing"});
    fs::remove_all(DIR);

    ASSERT_EQ(ENTRIES.size(), 2u);
    EXPECT_EQ(ENTRIES[0].first, "applet.desktop");
    EXPECT_EQ(ENTRIES[0].second.exec, "user-applet");
    EXPECT_TRUE(ENTRIES[0].second.hidden);
    EXPECT_EQ(ENTRIES[1].first, "other.desktop");
}
