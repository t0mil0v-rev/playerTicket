#pragma once
// tokenpizder | Author: DoubleLuc (https://github.com/t0mil0v-rev)
// Header for external C++ projects linking against lib/tokenpizder.lib

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <cstdint>
#include <optional>
#include <string>

namespace process {

struct target_info {
    DWORD       pid{};
    HANDLE      handle{};
    std::string exe_name;
};

[[nodiscard]] std::optional<target_info> check_emul() noexcept;
[[nodiscard]] std::optional<std::string> scan(HANDLE handle) noexcept;
[[nodiscard]] std::optional<std::string> get_ticket() noexcept;

} // namespace process
