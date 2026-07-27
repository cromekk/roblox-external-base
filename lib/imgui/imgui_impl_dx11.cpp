#include "imgui.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <cstdio>

#pragma comment(lib, "d3dcompiler.lib")

struct ImGui_ImplDX11_Data {
    ID3D11Device*             pd3dDevice;
    ID3D11DeviceContext*      pd3dDeviceContext;
    IDXGIFactory*             pFactory;
    ID3D11Buffer*             pVB;
    ID3D11Buffer*             pIB;
    ID3D11VertexShader*       pVertexShader;
    ID3D11InputLayout*        pInputLayout;
    ID3D11Buffer*             pVertexConstantBuffer;
    ID3D11PixelShader*        pPixelShader;
    ID3D11SamplerState*       pFontSampler;
    ID3D11ShaderResourceView* pFontTextureView;
    ID3D11RasterizerState*    pRasterizerState;
    ID3D11BlendState*         pBlendState;
    ID3D11DepthStencilState*  pDepthStencilState;
    int                       VertexBufferSize;
    int                       IndexBufferSize;
};

static ImGui_ImplDX11_Data* ImGui_ImplDX11_GetBackendData() {
    return ImGui::GetCurrentContext() ? (ImGui_ImplDX11_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

struct VERTEX_CONSTANT_BUFFER {
    float   mvp[4][4];
};

static void ImGui_ImplDX11_SetupRenderState(ImDrawData* draw_data, ID3D11DeviceContext* ctx) {
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();

    D3D11_VIEWPORT vp{};
    vp.Width    = draw_data->DisplaySize.x;
    vp.Height   = draw_data->DisplaySize.y;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = vp.TopLeftY = 0;
    ctx->RSSetViewports(1, &vp);

    unsigned int stride = sizeof(ImDrawVert);
    unsigned int offset = 0;
    ctx->IASetInputLayout(bd->pInputLayout);
    ctx->IASetVertexBuffers(0, 1, &bd->pVB, &stride, &offset);
    ctx->IASetIndexBuffer(bd->pIB, sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(bd->pVertexShader, nullptr, 0);
    ctx->VSSetConstantBuffers(0, 1, &bd->pVertexConstantBuffer);
    ctx->PSSetShader(bd->pPixelShader, nullptr, 0);
    ctx->PSSetSamplers(0, 1, &bd->pFontSampler);
    ctx->GSSetShader(nullptr, nullptr, 0);
    ctx->HSSetShader(nullptr, nullptr, 0);
    ctx->DSSetShader(nullptr, nullptr, 0);
    ctx->CSSetShader(nullptr, nullptr, 0);

    const float blend_factor[4] = { 0.f, 0.f, 0.f, 0.f };
    ctx->OMSetBlendState(bd->pBlendState, blend_factor, 0xffffffff);
    ctx->OMSetDepthStencilState(bd->pDepthStencilState, 0);
    ctx->RSSetState(bd->pRasterizerState);
}

void ImGui_ImplDX11_RenderDrawData(ImDrawData* draw_data) {
    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
        return;

    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    ID3D11DeviceContext* ctx = bd->pd3dDeviceContext;

    if (!bd->pVB || bd->VertexBufferSize < draw_data->TotalVtxCount) {
        if (bd->pVB) { bd->pVB->Release(); bd->pVB = nullptr; }
        bd->VertexBufferSize = draw_data->TotalVtxCount + 5000;
        D3D11_BUFFER_DESC desc{};
        desc.Usage          = D3D11_USAGE_DYNAMIC;
        desc.ByteWidth      = bd->VertexBufferSize * sizeof(ImDrawVert);
        desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (bd->pd3dDevice->CreateBuffer(&desc, nullptr, &bd->pVB) < 0)
            return;
    }
    if (!bd->pIB || bd->IndexBufferSize < draw_data->TotalIdxCount) {
        if (bd->pIB) { bd->pIB->Release(); bd->pIB = nullptr; }
        bd->IndexBufferSize = draw_data->TotalIdxCount + 10000;
        D3D11_BUFFER_DESC desc{};
        desc.Usage          = D3D11_USAGE_DYNAMIC;
        desc.ByteWidth      = bd->IndexBufferSize * sizeof(ImDrawIdx);
        desc.BindFlags      = D3D11_BIND_INDEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (bd->pd3dDevice->CreateBuffer(&desc, nullptr, &bd->pIB) < 0)
            return;
    }

    D3D11_MAPPED_SUBRESOURCE vtx_resource, idx_resource;
    if (ctx->Map(bd->pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &vtx_resource) != S_OK)
        return;
    if (ctx->Map(bd->pIB, 0, D3D11_MAP_WRITE_DISCARD, 0, &idx_resource) != S_OK) {
        ctx->Unmap(bd->pVB, 0);
        return;
    }

    ImDrawVert* vtx_dst = (ImDrawVert*)vtx_resource.pData;
    ImDrawIdx* idx_dst = (ImDrawIdx*)idx_resource.pData;
    for (int n = 0; n < draw_data->CmdListsCount; n++) {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtx_dst += cmd_list->VtxBuffer.Size;
        idx_dst += cmd_list->IdxBuffer.Size;
    }
    ctx->Unmap(bd->pVB, 0);
    ctx->Unmap(bd->pIB, 0);

    D3D11_MAPPED_SUBRESOURCE mapped_resource;
    if (ctx->Map(bd->pVertexConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource) != S_OK)
        return;
    VERTEX_CONSTANT_BUFFER* constant_buffer = (VERTEX_CONSTANT_BUFFER*)mapped_resource.pData;
    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
    float mvp[4][4] =
    {
        { 2.0f/(R-L),   0.0f,           0.0f,       0.0f },
        { 0.0f,         2.0f/(T-B),     0.0f,       0.0f },
        { 0.0f,         0.0f,           0.5f,       0.0f },
        { (R+L)/(L-R),  (T+B)/(B-T),    0.5f,       1.0f },
    };
    memcpy(&constant_buffer->mvp, mvp, sizeof(mvp));
    ctx->Unmap(bd->pVertexConstantBuffer, 0);

    ImGui_ImplDX11_SetupRenderState(draw_data, ctx);

    int global_idx_offset = 0;
    int global_vtx_offset = 0;
    ImVec2 clip_off = draw_data->DisplayPos;
    for (int n = 0; n < draw_data->CmdListsCount; n++) {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != nullptr) {
                pcmd->UserCallback(cmd_list, pcmd);
            }
            else {
                ImVec2 clip_min(pcmd->ClipRect.x - clip_off.x, pcmd->ClipRect.y - clip_off.y);
                ImVec2 clip_max(pcmd->ClipRect.z - clip_off.x, pcmd->ClipRect.w - clip_off.y);
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                const D3D11_RECT r = { (LONG)clip_min.x, (LONG)clip_min.y, (LONG)clip_max.x, (LONG)clip_max.y };
                ctx->RSSetScissorRects(1, &r);

                ID3D11ShaderResourceView* texture_srv = (ID3D11ShaderResourceView*)pcmd->GetTexID();
                ctx->PSSetShaderResources(0, 1, &texture_srv);
                ctx->DrawIndexed(pcmd->ElemCount, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset);
            }
        }
        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }
}

static void ImGui_ImplDX11_CreateFontsTexture() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width          = width;
    desc.Height         = height;
    desc.MipLevels      = 1;
    desc.ArraySize      = 1;
    desc.Format         = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage          = D3D11_USAGE_DEFAULT;
    desc.BindFlags      = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    ID3D11Texture2D* pTexture = nullptr;
    D3D11_SUBRESOURCE_DATA subResource{};
    subResource.pSysMem = pixels;
    subResource.SysMemPitch = desc.Width * 4;
    subResource.SysMemSlicePitch = 0;
    bd->pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);

    if (pTexture) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = desc.MipLevels;
        srvDesc.Texture2D.MostDetailedMip = 0;
        bd->pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, &bd->pFontTextureView);
        pTexture->Release();
    }

    io.Fonts->SetTexID((ImTextureID)bd->pFontTextureView);

    D3D11_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MipLODBias     = 0.f;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    samplerDesc.MinLOD         = 0.f;
    samplerDesc.MaxLOD         = 0.f;
    bd->pd3dDevice->CreateSamplerState(&samplerDesc, &bd->pFontSampler);
}

bool ImGui_ImplDX11_CreateDeviceObjects() {
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    if (!bd->pd3dDevice)
        return false;
    if (bd->pFontSampler)
        ImGui_ImplDX11_InvalidateDeviceObjects();

    static const char* vertex_shader_hlsl =
        "cbuffer vertexBuffer : register(b0) \
        {\
            float4x4 ProjectionMatrix; \
        };\
        struct VS_INPUT\
        {\
            float2 pos : POSITION;\
            float4 col : COLOR0;\
            float2 uv  : TEXCOORD0;\
        };\
        struct PS_INPUT\
        {\
            float4 pos : SV_POSITION;\
            float4 col : COLOR0;\
            float2 uv  : TEXCOORD0;\
        };\
        PS_INPUT main(VS_INPUT input)\
        {\
            PS_INPUT output;\
            output.pos = mul(ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\
            output.col = input.col;\
            output.uv  = input.uv;\
            return output;\
        }";

    static const char* pixel_shader_hlsl =
        "struct PS_INPUT\
        {\
            float4 pos : SV_POSITION;\
            float4 col : COLOR0;\
            float2 uv  : TEXCOORD0;\
        };\
        sampler sampler0;\
        Texture2D texture0;\
        float4 main(PS_INPUT input) : SV_Target\
        {\
            float4 out_col = input.col * texture0.Sample(sampler0, input.uv); \
            return out_col; \
        }";

    ID3DBlob* vertex_shader_blob = nullptr;
    ID3DBlob* pixel_shader_blob = nullptr;
    ID3DBlob* error_blob = nullptr;

    if (D3DCompile(vertex_shader_hlsl, strlen(vertex_shader_hlsl), nullptr, nullptr, nullptr, "main", "vs_4_0", 0, 0, &vertex_shader_blob, &error_blob) != S_OK)
        return false;
    if (D3DCompile(pixel_shader_hlsl, strlen(pixel_shader_hlsl), nullptr, nullptr, nullptr, "main", "ps_4_0", 0, 0, &pixel_shader_blob, &error_blob) != S_OK) {
        vertex_shader_blob->Release();
        return false;
    }

    bd->pd3dDevice->CreateVertexShader(vertex_shader_blob->GetBufferPointer(), vertex_shader_blob->GetBufferSize(), nullptr, &bd->pVertexShader);
    bd->pd3dDevice->CreatePixelShader(pixel_shader_blob->GetBufferPointer(), pixel_shader_blob->GetBufferSize(), nullptr, &bd->pPixelShader);

    D3D11_INPUT_ELEMENT_DESC local_layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)offsetof(ImDrawVert, pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)offsetof(ImDrawVert, uv),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, (UINT)offsetof(ImDrawVert, col), D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    bd->pd3dDevice->CreateInputLayout(local_layout, 3, vertex_shader_blob->GetBufferPointer(), vertex_shader_blob->GetBufferSize(), &bd->pInputLayout);

    vertex_shader_blob->Release();
    pixel_shader_blob->Release();

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth      = sizeof(VERTEX_CONSTANT_BUFFER);
    desc.Usage          = D3D11_USAGE_DYNAMIC;
    desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd->pd3dDevice->CreateBuffer(&desc, nullptr, &bd->pVertexConstantBuffer);

    D3D11_BLEND_DESC blend_desc{};
    blend_desc.AlphaToCoverageEnable = false;
    blend_desc.RenderTarget[0].BlendEnable = true;
    blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    bd->pd3dDevice->CreateBlendState(&blend_desc, &bd->pBlendState);

    D3D11_RASTERIZER_DESC rasterizer_desc{};
    rasterizer_desc.FillMode = D3D11_FILL_SOLID;
    rasterizer_desc.CullMode = D3D11_CULL_NONE;
    rasterizer_desc.ScissorEnable = true;
    rasterizer_desc.DepthClipEnable = true;
    bd->pd3dDevice->CreateRasterizerState(&rasterizer_desc, &bd->pRasterizerState);

    D3D11_DEPTH_STENCIL_DESC depth_stencil_desc{};
    depth_stencil_desc.DepthEnable = false;
    depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depth_stencil_desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    depth_stencil_desc.StencilEnable = false;
    depth_stencil_desc.FrontFace.StencilFailOp = depth_stencil_desc.FrontFace.StencilDepthFailOp = depth_stencil_desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    depth_stencil_desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    depth_stencil_desc.BackFace = depth_stencil_desc.FrontFace;
    bd->pd3dDevice->CreateDepthStencilState(&depth_stencil_desc, &bd->pDepthStencilState);

    ImGui_ImplDX11_CreateFontsTexture();
    return true;
}

void ImGui_ImplDX11_InvalidateDeviceObjects() {
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    if (!bd)
        return;

    if (bd->pFontSampler)           { bd->pFontSampler->Release(); bd->pFontSampler = nullptr; }
    if (bd->pFontTextureView)       { bd->pFontTextureView->Release(); bd->pFontTextureView = nullptr; ImGui::GetIO().Fonts->SetTexID(0); }
    if (bd->pIB)                    { bd->pIB->Release(); bd->pIB = nullptr; }
    if (bd->pVB)                    { bd->pVB->Release(); bd->pVB = nullptr; }
    if (bd->pBlendState)            { bd->pBlendState->Release(); bd->pBlendState = nullptr; }
    if (bd->pDepthStencilState)     { bd->pDepthStencilState->Release(); bd->pDepthStencilState = nullptr; }
    if (bd->pRasterizerState)       { bd->pRasterizerState->Release(); bd->pRasterizerState = nullptr; }
    if (bd->pPixelShader)           { bd->pPixelShader->Release(); bd->pPixelShader = nullptr; }
    if (bd->pVertexConstantBuffer)  { bd->pVertexConstantBuffer->Release(); bd->pVertexConstantBuffer = nullptr; }
    if (bd->pInputLayout)           { bd->pInputLayout->Release(); bd->pInputLayout = nullptr; }
    if (bd->pVertexShader)          { bd->pVertexShader->Release(); bd->pVertexShader = nullptr; }
}

bool ImGui_ImplDX11_Init(ID3D11Device* device, ID3D11DeviceContext* device_context) {
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplDX11_Data* bd = IM_NEW(ImGui_ImplDX11_Data)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_dx11";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    bd->pd3dDevice = device;
    bd->pd3dDeviceContext = device_context;
    return true;
}

void ImGui_ImplDX11_Shutdown() {
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    ImGui_ImplDX11_InvalidateDeviceObjects();
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    IM_DELETE(bd);
}

void ImGui_ImplDX11_NewFrame() {
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    if (!bd->pFontSampler)
        ImGui_ImplDX11_CreateDeviceObjects();
}
