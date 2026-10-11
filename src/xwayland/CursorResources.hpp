#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace CursorResources {
    // Preserve unrelated resource records and replace only the two cursor resources.
    std::optional<std::string> merge(std::string_view resources, std::string_view theme, int size);
}
