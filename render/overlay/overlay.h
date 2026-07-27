#pragma once
#include <windows.h>
#include <d3d11.h>

struct ImFont;

namespace overlay {
    inline HWND                    window_handle       = nullptr;
    inline HWND                    target_window       = nullptr;
    inline ID3D11Device*           d3d_device          = nullptr;
    inline ID3D11DeviceContext*    d3d_device_context  = nullptr;
    inline IDXGISwapChain*         dxgi_swap_chain     = nullptr;
    inline ID3D11RenderTargetView* render_target_view  = nullptr;
    inline bool                    menu_visible        = true;

    inline ImFont* default_font = nullptr;

    bool initialize();
    void shutdown();
    bool is_running();
    void begin_frame();
    void end_frame();
}
