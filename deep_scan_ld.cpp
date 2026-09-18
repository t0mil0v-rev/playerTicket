#include <Windows.h>
#include <TlHelp32.h>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>

bool is_hex_char(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

void scan_ld_any(DWORD pid, const char* name) {
    HANDLE handle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!handle) return;

    printf("[*] Relaxed System.Byte[] Scan on %s (PID %lu)...\n", name, pid);

    MEMORY_BASIC_INFORMATION mbi{};
    uintptr_t addr = 0;
    std::vector<uint8_t> buf;
    int matches = 0;

    while (VirtualQueryEx(handle, (LPCVOID)addr, &mbi, sizeof(mbi))) {
        if (mbi.State == MEM_COMMIT && (mbi.Type == MEM_PRIVATE || mbi.Type == MEM_MAPPED) &&
            !(mbi.Protect & PAGE_NOACCESS) && !(mbi.Protect & PAGE_GUARD))
        {
            if (buf.size() < mbi.RegionSize) {
                buf.resize(mbi.RegionSize);
            }

            SIZE_T got = 0;
            if (ReadProcessMemory(handle, mbi.BaseAddress, buf.data(), mbi.RegionSize, &got) && got >= 0x40) {
                for (size_t i = 0; i + 0x40 <= got; i += 4) { // 4-byte aligned
                    // Check if length is 32 at offset +0x10, +0x14, +0x18, +0x1C
                    for (size_t len_off : {0x10, 0x14, 0x18, 0x1C}) {
                        uint32_t len = *(uint32_t*)(buf.data() + i + len_off);
                        if (len == 32) {
                            size_t str_off = len_off + 4; // string right after length
                            if (i + str_off + 32 <= got) {
                                bool is_hex = true;
                                for (size_t j = 0; j < 32; ++j) {
                                    if (!is_hex_char((char)buf[i + str_off + j])) {
                                        is_hex = false;
                                        break;
                                    }
                                }
                                if (is_hex) {
                                    std::string token((char*)buf.data() + i + str_off, 32);
                                    uintptr_t match_addr = (uintptr_t)mbi.BaseAddress + i;
                                    const char* type_s = mbi.Type == MEM_PRIVATE ? "PRIV" : "MAP";
                                    printf("  [+] MATCH at 0x%016llX (%s, sz=0x%zX, len_off=0x%zX): %s\n",
                                           (unsigned long long)match_addr, type_s, mbi.RegionSize, len_off, token.c_str());
                                    matches++;
                                    if (matches >= 20) break;
                                }
                            }
                        }
                    }
                    if (matches >= 20) break;
                }
            }
        }
        if (matches >= 20) break;
        uintptr_t next = (uintptr_t)mbi.BaseAddress + (mbi.RegionSize ? mbi.RegionSize : 0x1000);
        if (next <= addr) break;
        addr = next;
    }

    printf("[*] Scan finished for %s. Total matches: %d\n", name, matches);
    CloseHandle(handle);
}

int main() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 entry{ sizeof(entry) };

    if (Process32First(snap, &entry)) {
        do {
            if (_stricmp(entry.szExeFile, "Ld9BoxHeadless.exe") == 0 ||
                _stricmp(entry.szExeFile, "dnplayer.exe") == 0)
            {
                scan_ld_any(entry.th32ProcessID, entry.szExeFile);
            }
        } while (Process32Next(snap, &entry));
    }

    CloseHandle(snap);
    return 0;
}
