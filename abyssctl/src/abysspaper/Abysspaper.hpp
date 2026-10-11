#pragma once

#include <expected>
#include <string>

namespace Abysspaper {
    std::expected<void, std::string> makeAbysspaperRequest(const std::string_view& rq);
};