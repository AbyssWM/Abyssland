#include "CursorResources.hpp"

#include <algorithm>

static bool isCursorResource(std::string_view record) {
    const auto COLON = record.find(':');
    if (COLON == std::string_view::npos)
        return false;

    auto key = record.substr(0, COLON);
    while (!key.empty() && (key.front() == ' ' || key.front() == '\t'))
        key.remove_prefix(1);
    while (!key.empty() && (key.back() == ' ' || key.back() == '\t'))
        key.remove_suffix(1);

    return key == "Xcursor.theme" || key == "Xcursor.size";
}

std::optional<std::string> CursorResources::merge(std::string_view resources, std::string_view theme, int size) {
    if (theme.empty() || size <= 0 || std::ranges::any_of(theme, [](unsigned char c) { return c < 0x20 || c == 0x7f; }))
        return std::nullopt;

    const bool TERMINATED = !resources.empty() && resources.back() == '\0';
    if (TERMINATED)
        resources.remove_suffix(1);
    if (resources.find('\0') != std::string_view::npos)
        return std::nullopt;

    std::string result;
    result.reserve(resources.size() + theme.size() + 64);
    for (size_t begin = 0; begin < resources.size();) {
        size_t end     = begin;
        auto   content = resources.substr(begin);
        while (!content.empty() && (content.front() == ' ' || content.front() == '\t'))
            content.remove_prefix(1);
        const bool COMMENT = !content.empty() && (content.front() == '!' || content.front() == '#');
        // Xrm joins physical lines ending in an unescaped backslash. Drop an entire
        // cursor record, including its continuation, while retaining all other bytes.
        while (end < resources.size()) {
            const auto NEWLINE = resources.find('\n', end);
            if (NEWLINE == std::string_view::npos) {
                end = resources.size();
                break;
            }

            size_t backslashes = 0;
            for (size_t pos = NEWLINE; pos > begin && resources[pos - 1] == '\\'; --pos)
                ++backslashes;
            end = NEWLINE + 1;
            if (COMMENT || backslashes % 2 == 0)
                break;
        }

        const auto RECORD = resources.substr(begin, end - begin);
        if (!isCursorResource(RECORD))
            result.append(RECORD);
        begin = end;
    }

    if (!result.empty()) {
        size_t end = result.size();
        if (result.back() == '\n')
            --end;
        size_t backslashes = 0;
        for (size_t pos = end; pos > 0 && result[pos - 1] == '\\'; --pos)
            ++backslashes;
        if (result.back() != '\n')
            result += '\n';
        // A dangling continuation must not consume the first new resource.
        if (backslashes % 2 != 0)
            result += '\n';
    }
    result += "Xcursor.theme: ";
    for (const char c : theme) {
        // Xrm interprets backslashes and trims unescaped leading whitespace.
        if (c == '\\')
            result += "\\\\";
        else if (c == ' ')
            result += "\\040";
        else
            result += c;
    }
    result += "\nXcursor.size: " + std::to_string(size) + '\n';
    if (TERMINATED)
        result += '\0';
    return result;
}
