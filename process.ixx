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

inline constexpr std::array<std::string_view, 13> KNOWN_EMULATORS = {
    "HD-Player.exe",       // BlueStacks 5
    "BlueStacks.exe",      // BlueStacks
    "BlueStacksX.exe",     // BlueStacks X
    "dnplayer.exe",        // LDPlayer 9 / 5 / 4
    "LDPlayer.exe",        // LDPlayer
    "LdBoxHeadless.exe",   // LDPlayer headless
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
inline std::optional<std::string> scan(HANDLE handle) noexcept {
    MEMORY_BASIC_INFORMATION mbi{};
    uintptr_t addr = 0;
    std::vector<uint8_t> buf;
    std::string found_token;

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
                                found_token.assign(reinterpret_cast<const char*>(buf.data() + i + 0x20), 32);
                                return found_token;
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
inline std::optional<std::string> get_ticket() noexcept {
    const auto target = check_emul();
    if (!target) return std::nullopt;
    const auto token = scan(target->handle);
    CloseHandle(target->handle);
    return token;
}

} // namespace process
