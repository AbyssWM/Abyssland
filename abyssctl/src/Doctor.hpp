#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

// `abyssctl doctor`: read-only report on what deviates from the Abyssland defaults.
// query: sends one request (e.g. "j/version") to the running instance and returns the reply, nullopt if unreachable.
// instanceSig: signature of the instance that query talks to (empty if none).
int runDoctor(bool json, const std::string& prefix, const std::string& sourceDir, const std::string& instanceSig,
              const std::function<std::optional<std::string>(std::string_view)>& query);
