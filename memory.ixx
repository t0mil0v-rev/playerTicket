module;

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>

export module memory;

export namespace mem {

inline HANDLE g_process = nullptr;

inline void initialize(HANDLE handle) noexcept {
    g_process = handle;
}

[[nodiscard]]
inline bool read_raw(std::uintptr_t addr, void* buf, std::size_t size) noexcept {
    if (!g_process || addr == 0 || addr <= 0x1000)
        return false;
    SIZE_T read{};
    return ReadProcessMemory(g_process, reinterpret_cast<LPCVOID>(addr), buf, size, &read)
        && read == size;
}

[[nodiscard]]
inline bool write_raw(std::uintptr_t addr, const void* buf, std::size_t size) noexcept {
    if (!g_process || addr == 0 || addr <= 0x1000)
        return false;
    SIZE_T written{};
    return WriteProcessMemory(g_process, reinterpret_cast<LPVOID>(addr), buf, size, &written)
        && written == size;
}

template<typename T>
    requires std::is_trivially_copyable_v<T>
[[nodiscard]]
inline T read(std::uintptr_t addr) noexcept {
    T val{};
    (void)read_raw(addr, &val, sizeof(T));
    return val;
}

template<typename T>
    requires std::is_trivially_copyable_v<T>
inline bool write(std::uintptr_t addr, const T& val) noexcept {
    return write_raw(addr, &val, sizeof(T));
}

[[nodiscard]]
inline std::optional<std::string> read_il2cpp_bytestring(std::uintptr_t obj) noexcept {
    if (!obj) return std::nullopt;
    const auto len = read<std::uint32_t>(obj + 0x18);
    if (len == 0 || len > 4096) return std::nullopt;
    std::string result(static_cast<std::size_t>(len), '\0');
    if (!read_raw(obj + 0x20, result.data(), len))
        return std::nullopt;
    return result;
}

[[nodiscard]]
inline std::optional<std::string> read_il2cpp_string(std::uintptr_t obj) noexcept {
    if (!obj) return std::nullopt;
    const auto len = read<std::int32_t>(obj + 0x10);
    if (len <= 0 || len > 4096) return std::nullopt;
    std::wstring wstr(static_cast<std::size_t>(len), L'\0');
    if (!read_raw(obj + 0x14, wstr.data(), static_cast<std::size_t>(len) * sizeof(wchar_t)))
        return std::nullopt;
    const int sz = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), len, nullptr, 0, nullptr, nullptr);
    if (sz <= 0) return std::nullopt;
    std::string result(static_cast<std::size_t>(sz), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), len, result.data(), sz, nullptr, nullptr);
    return result;
}

} // namespace mem
