#include "overlay.h"
#include "mem.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <dwmapi.h>
#include <tlhelp32.h>
#include <string>
#include <cctype>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace overlay {

static WNDCLASSEXW wc{};
static bool running = true;

static void create_render_target()
{
    ID3D11Texture2D* back_buffer = nullptr;
    dxgi_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    if (back_buffer) {
        d3d_device->CreateRenderTargetView(back_buffer, nullptr, &render_target_view);
        back_buffer->Release();
    }
}

static void release_render_target()
{
    if (render_target_view) {
        render_target_view->Release();
        render_target_view = nullptr;
    }
}

static LRESULT WINAPI wnd_proc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    if (ImGui_ImplWin32_WndProcHandler(h, m, wp, lp)) return true;
    switch (m) {
        case WM_SIZE:
            if (d3d_device && wp != SIZE_MINIMIZED) {
                release_render_target();
                dxgi_swap_chain->ResizeBuffers(0, LOWORD(lp), HIWORD(lp), DXGI_FORMAT_UNKNOWN, 0);
                create_render_target();
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            running = false;
            return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

struct find_ctx { DWORD pid; HWND best; int area; bool need_vis; };

static BOOL CALLBACK enum_by_pid(HWND h, LPARAM lp)
{
    find_ctx* c = (find_ctx*)lp;
    if (c->need_vis && !IsWindowVisible(h)) return TRUE;

    DWORD wp = 0;
    GetWindowThreadProcessId(h, &wp);
    if (wp != c->pid) return TRUE;

    RECT rc;
    if (!GetClientRect(h, &rc)) return TRUE;
    int a = (rc.right - rc.left) * (rc.bottom - rc.top);
    int s = a > 0 ? a : 1;
    if (s > c->area) { c->area = s; c->best = h; }
    return TRUE;
}

struct tenum_ctx { HWND best; int area; };
static BOOL CALLBACK enum_thread_wnd(HWND h, LPARAM lp)
{
    tenum_ctx* c = (tenum_ctx*)lp;
    RECT rc;
    if (!GetClientRect(h, &rc)) return TRUE;
    int a = (rc.right - rc.left) * (rc.bottom - rc.top);
    int s = a > 0 ? a : 1;
    if (s > c->area) { c->area = s; c->best = h; }
    return TRUE;
}

static HWND find_by_thread_enum(DWORD pid)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return NULL;

    THREADENTRY32 te{};
    te.dwSize = sizeof(te);
    HWND best = NULL;
    int best_area = 0;

    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID != pid) continue;
            tenum_ctx tc{ NULL, 0 };
            EnumThreadWindows(te.th32ThreadID, enum_thread_wnd, (LPARAM)&tc);
            if (tc.area > best_area) { best_area = tc.area; best = tc.best; }
        }
        while (Thread32Next(snap, &te));
    }
    CloseHandle(snap);
    return best;
}

static std::string exe_name_lower(DWORD pid)
{
    if (!pid) return {};
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return {};
    PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
    std::string out;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (pe.th32ProcessID == pid) {
                for (const wchar_t* p = pe.szExeFile; *p; ++p)
                    out.push_back((char)tolower((unsigned char)(*p & 0x7F)));
                break;
            }
        }
        while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return out;
}

struct title_ctx { HWND best; int area; DWORD pid; HWND ignore; };

static BOOL CALLBACK enum_by_title(HWND h, LPARAM lp)
{
    title_ctx* c = (title_ctx*)lp;
    if (h == c->ignore) return TRUE;
    if (!IsWindowVisible(h)) return TRUE;

    char title[128] = { 0 };
    int len = GetWindowTextA(h, title, sizeof(title));
    if (len <= 0) return TRUE;

    int a = 0, b = len - 1;
    while (a <= b && (title[a] == ' ' || title[a] == '\t')) ++a;
    while (b >= a && (title[b] == ' ' || title[b] == '\t')) --b;
    int tl = b - a + 1;
    if (tl <= 0 || tl > 64) return TRUE;

    bool hit = false;
    for (int i = a; i + 6 <= a + tl; ++i) {
        if (_strnicmp(title + i, "roblox", 6) == 0) { hit = true; break; }
    }
    if (!hit) return TRUE;

    RECT rc;
    if (!GetClientRect(h, &rc)) return TRUE;
    int cw = rc.right - rc.left;
    int ch = rc.bottom - rc.top;
    if (cw < 300 || ch < 200) return TRUE;

    DWORD wp = 0;
    GetWindowThreadProcessId(h, &wp);
    if (!wp) return TRUE;

    std::string exe = exe_name_lower(wp);
    if (exe.rfind("roblox", 0) != 0) return TRUE;
    if (exe.find("studio") != std::string::npos) return TRUE;

    int ar = cw * ch;
    if (ar > c->area) { c->area = ar; c->best = h; c->pid = wp; }
    return TRUE;
}

static HWND find_by_title(DWORD* out_pid)
{
    title_ctx c{ NULL, 0, 0, window_handle };
    EnumWindows(enum_by_title, (LPARAM)&c);
    if (out_pid) *out_pid = c.pid;
    return c.best;
}

static HWND find_roblox()
{
    DWORD pid = memory::process_id;

    if (pid != 0) {
        find_ctx c1{ pid, NULL, 0, true };
        EnumWindows(enum_by_pid, (LPARAM)&c1);
        if (c1.best) return c1.best;
    }

    {
        DWORD found = 0;
        HWND h = find_by_title(&found);
        if (h) {
            if (found && found != memory::process_id) memory::process_id = found;
            return h;
        }
    }

    if (pid != 0) {
        find_ctx c2{ pid, NULL, 0, false };
        EnumWindows(enum_by_pid, (LPARAM)&c2);
        if (c2.best) return c2.best;

        HWND h = find_by_thread_enum(pid);
        if (h) return h;
    }

    return NULL;
}

bool initialize()
{
    target_window = find_roblox();
    if (!target_window) { printf("[-] roblox window not found\n"); return false; }

    wc = { sizeof(wc), CS_CLASSDC, wnd_proc, 0, 0, GetModuleHandleW(NULL), NULL, NULL, NULL, NULL, L"zsov", NULL };
    RegisterClassExW(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    window_handle = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
        wc.lpszClassName, L"", WS_POPUP,
        0, 0, sw, sh,
        NULL, NULL, wc.hInstance, NULL);
    if (!window_handle) { printf("[-] CreateWindowExW failed err=%lu\n", GetLastError()); return false; }

    SetLayeredWindowAttributes(window_handle, 0, 255, LWA_ALPHA);
    MARGINS m = { -1 };
    DwmExtendFrameIntoClientArea(window_handle, &m);

    DXGI_SWAP_CHAIN_DESC swap_chain_desc{};
    swap_chain_desc.BufferCount = 2;
    swap_chain_desc.BufferDesc.Width = 0;
    swap_chain_desc.BufferDesc.Height = 0;
    swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
    swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
    swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.OutputWindow = window_handle;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.Windowed = TRUE;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL feature_level;
    const D3D_FEATURE_LEVEL feature_level_array[2] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };

    if (D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            feature_level_array,
            2,
            D3D11_SDK_VERSION,
            &swap_chain_desc,
            &dxgi_swap_chain,
            &d3d_device,
            &feature_level,
            &d3d_device_context) != S_OK)
        return false;

    create_render_target();

    ShowWindow(window_handle, SW_SHOWDEFAULT);
    UpdateWindow(window_handle);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    {
        char wdir[MAX_PATH];
        UINT n = GetWindowsDirectoryA(wdir, MAX_PATH);
        if (n > 0 && n < MAX_PATH) {
            std::string fonts = std::string(wdir) + "\\Fonts\\";
            auto try_load = [&](const char* file, float sz) -> ImFont* {
                std::string p = fonts + file;
                if (GetFileAttributesA(p.c_str()) == INVALID_FILE_ATTRIBUTES) return nullptr;
                return io.Fonts->AddFontFromFileTTF(p.c_str(), sz);
            };
            default_font = try_load("segoeui.ttf", 15.f);
            if (!default_font) default_font = try_load("bahnschrift.ttf", 15.f);
            if (default_font) io.FontDefault = default_font;
        }
    }

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(window_handle);
    ImGui_ImplDX11_Init(d3d_device, d3d_device_context);
    return true;
}

void shutdown()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    release_render_target();
    if (dxgi_swap_chain)    { dxgi_swap_chain->Release();    dxgi_swap_chain    = nullptr; }
    if (d3d_device_context) { d3d_device_context->Release(); d3d_device_context = nullptr; }
    if (d3d_device)         { d3d_device->Release();         d3d_device         = nullptr; }
    if (window_handle) DestroyWindow(window_handle);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
}

bool is_running()
{
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (!IsWindow(target_window)) {
        target_window = find_roblox();
        if (!target_window) return false;
    }

    RECT rc; GetClientRect(target_window, &rc);
    POINT tl{ 0, 0 }; ClientToScreen(target_window, &tl);
    int rw = rc.right - rc.left;
    int rh = rc.bottom - rc.top;
    if (rw > 0 && rh > 0 && !IsIconic(target_window)) {
        SetWindowPos(window_handle, HWND_TOPMOST, tl.x, tl.y, rw, rh, SWP_NOACTIVATE | SWP_NOSENDCHANGING);
    }

    LONG ex = GetWindowLongW(window_handle, GWL_EXSTYLE);
    if (menu_visible) SetWindowLongW(window_handle, GWL_EXSTYLE, ex & ~WS_EX_TRANSPARENT);
    else              SetWindowLongW(window_handle, GWL_EXSTYLE, ex |  WS_EX_TRANSPARENT);

    return running;
}

void begin_frame()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void end_frame()
{
    ImGui::EndFrame();
    ImGui::Render();

    const float clear_color[4] = { 0.f, 0.f, 0.f, 0.f };
    d3d_device_context->OMSetRenderTargets(1, &render_target_view, nullptr);
    d3d_device_context->ClearRenderTargetView(render_target_view, clear_color);

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    dxgi_swap_chain->Present(1, 0);
}

}
