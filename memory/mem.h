#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

namespace memory {

    inline HANDLE       process_handle = nullptr;
    inline DWORD        process_id     = 0;
    inline uintptr_t    base_address   = 0;
    inline std::wstring process_path;

    bool attach_process(const wchar_t* process_name);
    void detach_process();

    template <typename T>
    T read(uintptr_t address) {
        T value{};
        if (process_handle) {
            ReadProcessMemory(process_handle, reinterpret_cast<LPCVOID>(address), &value, sizeof(T), nullptr);
        }
        return value;
    }

    template <typename T>
    bool write(uintptr_t address, const T& value) {
        if (!process_handle) return false;
        return WriteProcessMemory(process_handle, reinterpret_cast<LPVOID>(address), &value, sizeof(T), nullptr) != 0;
    }

    std::string read_string(uintptr_t address);
    std::string read_roblox_string(uintptr_t address);
}
