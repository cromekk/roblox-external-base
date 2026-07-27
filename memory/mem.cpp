#include "mem.h"
#include <tlhelp32.h>

namespace memory {

static DWORD find_process_id(const wchar_t* process_name)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    DWORD result_pid = 0;

    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, process_name) == 0) {
                result_pid = entry.th32ProcessID;
                break;
            }
        }
        while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return result_pid;
}

static uintptr_t find_base_address(DWORD process_id, std::wstring* output_path)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, process_id);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    uintptr_t base_addr = 0;

    if (Module32FirstW(snapshot, &entry)) {
        base_addr = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
        if (output_path) *output_path = entry.szExePath;
    }
    CloseHandle(snapshot);
    return base_addr;
}

bool attach_process(const wchar_t* process_name)
{
    process_id = find_process_id(process_name);
    if (!process_id) return false;

    process_handle = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION, FALSE, process_id);
    if (!process_handle) return false;

    base_address = find_base_address(process_id, &process_path);
    return base_address != 0;
}

void detach_process()
{
    if (process_handle) CloseHandle(process_handle);
    process_handle = nullptr;
    process_id     = 0;
    base_address   = 0;
    process_path.clear();
}

std::string read_string(uintptr_t address)
{
    char buffer[128]{};
    ReadProcessMemory(process_handle, reinterpret_cast<LPCVOID>(address), buffer, sizeof(buffer) - 1, nullptr);
    return std::string(buffer);
}

std::string read_roblox_string(uintptr_t address)
{
    size_t length = read<size_t>(address + 0x10);
    if (length == 0 || length > 256) return {};

    uintptr_t data_address = (length >= 16) ? read<uintptr_t>(address) : address;

    std::string result;
    result.resize(length);
    ReadProcessMemory(process_handle, reinterpret_cast<LPCVOID>(data_address), result.data(), length, nullptr);
    return result;
}

}
