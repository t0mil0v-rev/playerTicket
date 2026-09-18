module;

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <cstdint>
#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include <array>
#include <chrono>
#include <cstdio>

export module process;

export namespace process {

struct target_info {
    DWORD       pid{};
    HANDLE      handle{};
    std::string exe_name;
};

inline constexpr std::array<std::string_view, 14> KNOWN_EMULATORS = {
    "HD-Player.exe",       // BlueStacks 5
    "Ld9BoxHeadless.exe",   // LDPlayer 9 VM Headless
    "dnplayer.exe",        // LDPlayer 9 / 5 / 4 Launcher
    "LDPlayer.exe",        // LDPlayer main
    "LdBoxHeadless.exe",   // LDPlayer older headless
    "BlueStacks.exe",      // BlueStacks
    "BlueStacksX.exe",     // BlueStacks X
    "NemuPlayer.exe",      // MuMu Player 12
    "NemuHeadless.exe",    // MuMu Player headless
    "MuMuPlayer.exe",      // MuMu Player
    "Nox.exe",             // Nox Player
    "NoxVMHandle.exe",     // Nox VM
    "MEmu.exe",            // MEmu Play
    "MEmuHeadless.exe"     // MEmu headless
};

[[nodiscard]]
constexpr bool is_hex_char(char c) noexcept {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

[[nodiscard]]
inline bool is_authentic_token(std::string_view s) noexcept {
    if (s.length() != 32) return false;
    uint32_t mask = 0;
    size_t unique_count = 0;

    for (char c : s) {
        if (!is_hex_char(c)) return false;
        uint32_t val = (c >= '0' && c <= '9') ? (c - '0') : (10 + c - 'a');
        if (!(mask & (1u << val))) {
            mask |= (1u << val);
            unique_count++;
        }
    }
    // Authentic Standoff 2 session tickets have high entropy (at least 8 distinct hex chars)
    return unique_count >= 8;
}

[[nodiscard]]
inline std::optional<target_info> check_emul() noexcept {
    const HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return std::nullopt;

    PROCESSENTRY32 entry{ .dwSize = sizeof(PROCESSENTRY32) };
    for (bool ok = Process32First(snap, &entry); ok; ok = Process32Next(snap, &entry)) {
        for (const auto& emu : KNOWN_EMULATORS) {
            if (_stricmp(entry.szExeFile, emu.data()) == 0) {
                const DWORD pid = entry.th32ProcessID;
                const HANDLE handle = OpenProcess(
                    PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
                    FALSE, pid
                );
                if (handle && handle != INVALID_HANDLE_VALUE) {
                    CloseHandle(snap);
                    return target_info{
                        .pid = pid,
                        .handle = handle,
                        .exe_name = std::string(emu)
                    };
                }
            }
        }
    }
    CloseHandle(snap);
    return std::nullopt;
}

[[nodiscard]]
inline std::optional<std::string> scan_bluestacks(HANDLE handle) noexcept {
    MEMORY_BASIC_INFORMATION mbi{};
    uintptr_t addr = 0;
    std::vector<uint8_t> buf;

    while (VirtualQueryEx(handle, reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi))) {
        if (mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE &&
            (mbi.Protect == PAGE_READWRITE || mbi.Protect == PAGE_EXECUTE_READWRITE) &&
            mbi.RegionSize >= 64 * 1024)
        {
            if (buf.size() < mbi.RegionSize) {
                buf.resize(mbi.RegionSize);
            }

            SIZE_T bytes_read = 0;
            if (ReadProcessMemory(handle, mbi.BaseAddress, buf.data(), mbi.RegionSize, &bytes_read)
                && bytes_read >= 64)
            {
                for (size_t i = 0; i + 64 <= bytes_read; i += 8) {
                    const uint32_t len = *reinterpret_cast<const uint32_t*>(buf.data() + i + 0x18);
                    if (len == 32 && *reinterpret_cast<const uint32_t*>(buf.data() + i + 0x1C) == 0) {
                        const uint64_t mon = *reinterpret_cast<const uint64_t*>(buf.data() + i + 0x08);
                        const uint64_t bnd = *reinterpret_cast<const uint64_t*>(buf.data() + i + 0x10);
                        if (mon == 0 && bnd == 0) {
                            bool is_valid_hex = true;
                            for (size_t j = 0; j < 32; ++j) {
                                if (!is_hex_char(static_cast<char>(buf[i + 0x20 + j]))) {
                                    is_valid_hex = false;
                                    break;
                                }
                            }
                            if (is_valid_hex) {
                                return std::string(reinterpret_cast<const char*>(buf.data() + i + 0x20), 32);
                            }
                        }
                    }
                }
            }
        }

        const uintptr_t next = reinterpret_cast<uintptr_t>(mbi.BaseAddress)
                             + (mbi.RegionSize ? mbi.RegionSize : 0x1000);
        if (next <= addr) break;
        addr = next;
    }

    return std::nullopt;
}

[[nodiscard]]
inline std::optional<std::string> scan_ldplayer(HANDLE handle) noexcept {
    MEMORY_BASIC_INFORMATION mbi{};
    uintptr_t addr = 0;
    std::vector<uint8_t> buf;

    while (VirtualQueryEx(handle, reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi))) {
        if (mbi.State == MEM_COMMIT && (mbi.Type == MEM_PRIVATE || mbi.Type == MEM_MAPPED) &&
            !(mbi.Protect & PAGE_NOACCESS) && !(mbi.Protect & PAGE_GUARD))
        {
            if (buf.size() < mbi.RegionSize) {
                buf.resize(mbi.RegionSize);
            }

            SIZE_T bytes_read = 0;
            if (ReadProcessMemory(handle, mbi.BaseAddress, buf.data(), mbi.RegionSize, &bytes_read)
                && bytes_read >= 64)
            {
                // Pass 1: Standard 8-byte aligned System.Byte[] object
                for (size_t i = 0; i + 64 <= bytes_read; i += 8) {
                    const uint32_t len = *reinterpret_cast<const uint32_t*>(buf.data() + i + 0x18);
                    const uint32_t len_hi = *reinterpret_cast<const uint32_t*>(buf.data() + i + 0x1C);
                    if (len == 32 && len_hi == 0) {
                        const uint64_t mon = *reinterpret_cast<const uint64_t*>(buf.data() + i + 0x08);
                        const uint64_t bnd = *reinterpret_cast<const uint64_t*>(buf.data() + i + 0x10);
                        if (mon == 0 && bnd == 0) {
                            std::string candidate(reinterpret_cast<const char*>(buf.data() + i + 0x20), 32);
                            if (is_authentic_token(candidate)) {
                                return candidate;
                            }
                        }
                    }
                }

                // Pass 2: LDPlayer 4-byte aligned System.Byte[] candidate scan
                for (size_t i = 0; i + 48 <= bytes_read; i += 4) {
                    const uint32_t len = *reinterpret_cast<const uint32_t*>(buf.data() + i + 0x1C);
                    if (len == 32) {
                        std::string candidate(reinterpret_cast<const char*>(buf.data() + i + 0x20), 32);
                        if (is_authentic_token(candidate)) {
                            return candidate;
                        }
                    }
                }
            }
        }

        const uintptr_t next = reinterpret_cast<uintptr_t>(mbi.BaseAddress)
                             + (mbi.RegionSize ? mbi.RegionSize : 0x1000);
        if (next <= addr) break;
        addr = next;
    }

    return std::nullopt;
}

[[nodiscard]]
inline std::optional<std::string> scan(const target_info& target) noexcept {
    if (target.exe_name.find("HD-Player") != std::string::npos ||
        target.exe_name.find("BlueStacks") != std::string::npos)
    {
        return scan_bluestacks(target.handle);
    }
    return scan_ldplayer(target.handle);
}

[[nodiscard]]
inline std::optional<std::string> get_ticket() noexcept {
    const auto target = check_emul();
    if (!target) return std::nullopt;
    const auto token = scan(*target);
    CloseHandle(target->handle);
    return token;
}

} // namespace process
