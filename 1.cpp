#include <windows.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <d3dcompiler.h> // 🎯 魔改後端編譯著色器必備

// 指向 imgui 資料夾
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib") // 🎯 連結著色器編譯程式庫

struct DetectedTarget {
    float x, y, w, h;
};

// ─── DirectX 11 全域變數 ───
HWND g_hWnd = nullptr;
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

std::atomic<bool> g_Running(false);
std::atomic<bool> g_Initialized(false); 

bool g_ShowMenu = true;       
bool g_AimbotState = false;   

bool g_EspBox = false;
bool g_EspLine = false;
float g_AimbotFov = 90.0f;
float g_AimbotSmooth = 5.0f; 
int g_TargetBone = 0; 
const char* g_BoneNames[] = { u8"骨骼: 頭部 (Head)", u8"骨骼: 胸口 (Chest)", u8"骨骼: 腹部 (Pelvis)" };
float g_BoxColor[4] = { 0.0f, 0.48f, 1.0f, 1.0f }; 

std::vector<DetectedTarget> g_DetectedTargets;
std::mutex g_TargetMutex;

int g_CurrentTab = 0; 
std::thread g_RenderThread;   

// ─── 完美修復版：本地 ImGui DX11 後端數據與渲染管線 ───
struct ImGui_ImplDX11_Data {
    ID3D11Device* pd3dDevice;
    ID3D11DeviceContext* pd3dDeviceContext;
    ID3D11Buffer* pVB;
    ID3D11Buffer* pIB;
    ID3D11VertexShader* pVertexShader;
    ID3D11InputLayout* pInputLayout;
    ID3D11Buffer* pVertexConstantBuffer;
    ID3D11PixelShader* pPixelShader;
    ID3D11SamplerState* pFontSampler;
    ID3D11ShaderResourceView* pFontTextureView;
    ID3D11RasterizerState* pRasterizerState;
    ID3D11BlendState* pBlendState;
    ID3D11DepthStencilState* pDepthStencilState;
    int VertexBufferSize;
    int IndexBufferSize;
    ImGui_ImplDX11_Data() { memset(this, 0, sizeof(*this)); VertexBufferSize = 5000; IndexBufferSize = 10000; }
};

struct VERTEX_CONSTANT_BUFFER {
    float mvp[4][4];
};

static ImGui_ImplDX11_Data* ImGui_ImplDX11_GetBackendData() {
    return ImGui::GetCurrentContext() ? (ImGui_ImplDX11_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

IMGUI_IMPL_API bool ImGui_ImplDX11_Init(ID3D11Device* device, ID3D11DeviceContext* device_context) {
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Renderer backend already initialized!");
    ImGui_ImplDX11_Data* bd = IM_NEW(ImGui_ImplDX11_Data)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_dx11_xuans_fixed";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    bd->pd3dDevice = device;
    bd->pd3dDeviceContext = device_context;
    return true;
}

static void ImGui_ImplDX11_DestroyDeviceObjects() {
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    if (!bd) return;
    if (bd->pVB) { bd->pVB->Release(); bd->pVB = nullptr; }
    if (bd->pIB) { bd->pIB->Release(); bd->pIB = nullptr; }
    if (bd->pVertexShader) { bd->pVertexShader->Release(); bd->pVertexShader = nullptr; }
    if (bd->pInputLayout) { bd->pInputLayout->Release(); bd->pInputLayout = nullptr; }
    if (bd->pVertexConstantBuffer) { bd->pVertexConstantBuffer->Release(); bd->pVertexConstantBuffer = nullptr; }
    if (bd->pPixelShader) { bd->pPixelShader->Release(); bd->pPixelShader = nullptr; }
    if (bd->pFontSampler) { bd->pFontSampler->Release(); bd->pFontSampler = nullptr; }
    if (bd->pFontTextureView) { bd->pFontTextureView->Release(); bd->pFontTextureView = nullptr; ImGui::GetIO().Fonts->SetTexID(nullptr); }
    if (bd->pRasterizerState) { bd->pRasterizerState->Release(); bd->pRasterizerState = nullptr; }
    if (bd->pBlendState) { bd->pBlendState->Release(); bd->pBlendState = nullptr; }
    if (bd->pDepthStencilState) { bd->pDepthStencilState->Release(); bd->pDepthStencilState = nullptr; }
}

IMGUI_IMPL_API void ImGui_ImplDX11_Shutdown() {
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    if (!bd) return;
    ImGui_ImplDX11_DestroyDeviceObjects();
    ImGui::GetIO().BackendRendererUserData = nullptr;
    IM_DELETE(bd);
}

static void ImGui_ImplDX11_CreateDeviceObjects() {
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    if (!bd || bd->pFontTextureView) return;

    // 1. 建立字體紋理
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels; int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width; desc.Height = height; desc.MipLevels = 1; desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA subResource = { pixels, (UINT)(width * 4), 0 };
    ID3D11Texture2D* pTexture = nullptr;
    bd->pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    bd->pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, &bd->pFontTextureView);
    pTexture->Release();
    io.Fonts->SetTexID((ImTextureID)bd->pFontTextureView);

    // 2. 建立採樣器
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP; samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    bd->pd3dDevice->CreateSamplerState(&samplerDesc, &bd->pFontSampler);

    // 3. 內嵌著色器代碼 (HLSL)
    const char* vertexShaderCode = 
        "cbuffer vertexBuffer : register(b0) { float4x4 ProjectionMatrix; };\n"
        "struct VS_INPUT { float2 pos : POSITION; float2 uv  : TEXCOORD0; float4 col : COLOR0; };\n"
        "struct VS_OUTPUT { float4 pos : SV_POSITION; float4 col : COLOR0; float2 uv  : TEXCOORD0; };\n"
        "VS_OUTPUT main(VS_INPUT input) {\n"
        "    VS_OUTPUT output;\n"
        "    output.pos = mul(ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\n"
        "    output.col = input.col;\n"
        "    output.uv  = input.uv;\n"
        "    return output;\n"
        "}";

    const char* pixelShaderCode = 
        "struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR0; float2 uv  : TEXCOORD0; };\n"
        "sampler sampler0 : register(s0);\n"
        "Texture2D texture0 : register(t0);\n"
        "float4 main(PS_INPUT input) : SV_Target {\n"
        "    return input.col * texture0.Sample(sampler0, input.uv);\n"
        "}";

    // 4. 編譯著色器
    ID3DBlob* vsBlob = nullptr; ID3DBlob* psBlob = nullptr;
    D3DCompile(vertexShaderCode, strlen(vertexShaderCode), nullptr, nullptr, nullptr, "main", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(pixelShaderCode, strlen(pixelShaderCode), nullptr, nullptr, nullptr, "main", "ps_4_0", 0, 0, &psBlob, nullptr);

    bd->pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &bd->pVertexShader);
    bd->pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &bd->pPixelShader);

    // 5. 建立 Input Layout
    D3D11_INPUT_ELEMENT_DESC local_layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)offsetof(ImDrawVert, pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)offsetof(ImDrawVert, uv),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, (UINT)offsetof(ImDrawVert, col), D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    bd->pd3dDevice->CreateInputLayout(local_layout, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &bd->pInputLayout);
    vsBlob->Release(); psBlob->Release();

    // 6. 建立常數緩衝區 (Constant Buffer)
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(VERTEX_CONSTANT_BUFFER); cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd->pd3dDevice->CreateBuffer(&cbDesc, nullptr, &bd->pVertexConstantBuffer);

    // 7. 渲染狀態設定 (Blend, Depth, Rasterizer)
    D3D11_BLEND_DESC blend_desc = {};
    blend_desc.RenderTarget[0].BlendEnable = TRUE;
    blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD; blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA; blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    bd->pd3dDevice->CreateBlendState(&blend_desc, &bd->pBlendState);

    D3D11_DEPTH_STENCIL_DESC depth_desc = {};
    bd->pd3dDevice->CreateDepthStencilState(&depth_desc, &bd->pDepthStencilState);

    D3D11_RASTERIZER_DESC raster_desc = {};
    raster_desc.FillMode = D3D11_FILL_SOLID; raster_desc.CullMode = D3D11_CULL_NONE; raster_desc.ScissorEnable = TRUE;
    bd->pd3dDevice->CreateRasterizerState(&raster_desc, &bd->pRasterizerState);
}

IMGUI_IMPL_API void ImGui_ImplDX11_NewFrame() {
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    if (!bd) return;
    if (!bd->pFontTextureView) ImGui_ImplDX11_CreateDeviceObjects();
}

// 🎯 核心修正：將被閹割掉的實際繪圖流程 (Draw Call) 完全實作出來
IMGUI_IMPL_API void ImGui_ImplDX11_RenderDrawData(ImDrawData* draw_data) {
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    if (!bd || draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f) return;

    ID3D11DeviceContext* ctx = bd->pd3dDeviceContext;

    // 建立或動態調整頂點緩衝區大小
    if (!bd->pVB || bd->VertexBufferSize < draw_data->TotalVtxCount) {
        if (bd->pVB) bd->pVB->Release();
        bd->VertexBufferSize = draw_data->TotalVtxCount + 5000;
        D3D11_BUFFER_DESC desc = { (UINT)(bd->VertexBufferSize * sizeof(ImDrawVert)), D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
        bd->pd3dDevice->CreateBuffer(&desc, nullptr, &bd->pVB);
    }
    // 建立或動態調整索引緩衝區大小
    if (!bd->pIB || bd->IndexBufferSize < draw_data->TotalIdxCount) {
        if (bd->pIB) bd->pIB->Release();
        bd->IndexBufferSize = draw_data->TotalIdxCount + 10000;
        D3D11_BUFFER_DESC desc = { (UINT)(bd->IndexBufferSize * sizeof(ImDrawIdx)), D3D11_USAGE_DYNAMIC, D3D11_BIND_INDEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
        bd->pd3dDevice->CreateBuffer(&desc, nullptr, &bd->pIB);
    }

    // 拷貝頂點與索引資料到顯示卡
    D3D11_MAPPED_SUBRESOURCE vtx_resource, idx_resource;
    if (ctx->Map(bd->pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &vtx_resource) != S_OK) return;
    if (ctx->Map(bd->pIB, 0, D3D11_MAP_WRITE_DISCARD, 0, &idx_resource) != S_OK) return;
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

    // 設定正交投影矩陣 (MVP Matrix)
    {
        D3D11_MAPPED_SUBRESOURCE mapped_resource;
        if (ctx->Map(bd->pVertexConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource) == S_OK) {
            VERTEX_CONSTANT_BUFFER* constant_buffer = (VERTEX_CONSTANT_BUFFER*)mapped_resource.pData;
            float L = draw_data->DisplayPos.x;
            float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
            float T = draw_data->DisplayPos.y;
            float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
            float mvp[4][4] = {
                { 2.0f/(R-L),   0.0f,           0.0f,       0.0f },
                { 0.0f,         2.0f/(T-B),     0.0f,       0.0f },
                { 0.0f,         0.0f,           0.5f,       0.0f },
                { (R+L)/(L-R),  (T+B)/(B-T),    0.5f,       1.0f },
            };
            memcpy(&constant_buffer->mvp, mvp, sizeof(mvp));
            ctx->Unmap(bd->pVertexConstantBuffer, 0);
        }
    }

    // 設置 DX11 渲染管道狀態
    UINT stride = sizeof(ImDrawVert); UINT offset = 0;
    ctx->IASetVertexBuffers(0, 1, &bd->pVB, &stride, &offset);
    ctx->IASetIndexBuffer(bd->pIB, sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->IASetInputLayout(bd->pInputLayout);
    ctx->VSSetShader(bd->pVertexShader, nullptr, 0);
    ctx->VSSetConstantBuffers(0, 1, &bd->pVertexConstantBuffer);
    ctx->PSSetShader(bd->pPixelShader, nullptr, 0);
    ctx->PSSetSamplers(0, 1, &bd->pFontSampler);
    ctx->OMSetBlendState(bd->pBlendState, nullptr, 0xffffffff);
    ctx->OMSetDepthStencilState(bd->pDepthStencilState, 0);
    ctx->RSSetState(bd->pRasterizerState);

    // 遍歷並執行繪圖指令
    int global_vtx_offset = 0; int global_idx_offset = 0;
    ImVec2 clip_off = draw_data->DisplayPos;
    for (int n = 0; n < draw_data->CmdListsCount; n++) {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            
            // 裁剪視窗設定
            D3D11_RECT r = { (LONG)(pcmd->ClipRect.x - clip_off.x), (LONG)(pcmd->ClipRect.y - clip_off.y), (LONG)(pcmd->ClipRect.z - clip_off.x), (LONG)(pcmd->ClipRect.w - clip_off.y) };
            ctx->RSSetScissorRects(1, &r);

            // 綁定紋理並開畫
            ID3D11ShaderResourceView* texture_srv = (ID3D11ShaderResourceView*)pcmd->GetTexID();
            ctx->PSSetShaderResources(0, 1, &texture_srv);
            ctx->DrawIndexed(pcmd->ElemCount, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset);
        }
        global_vtx_offset += cmd_list->VtxBuffer.Size;
        global_idx_offset += cmd_list->IdxBuffer.Size;
    }
}

// ─── 視窗核心功能代碼 ───
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; 
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; 
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
    return true;
}

void CleanupDeviceD3D() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg) {
        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0) == SC_KEYMENU) return 0;
            break;
        case WM_CLOSE: g_Running = false; return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
        default: return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void RenderLoop() {
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_HREDRAW | CS_VREDRAW, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"XUANS_Overlay", nullptr };
    RegisterClassExW(&wc);

    g_hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED,
        wc.lpszClassName, L"XUANS D3D11 Overlay",
        WS_POPUP,
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        nullptr, nullptr, wc.hInstance, nullptr
    );

    if (!g_hWnd) { g_Running = false; return; }

    SetLayeredWindowAttributes(g_hWnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
    
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(g_hWnd, &margins);

    if (!CreateDeviceD3D(g_hWnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        g_Running = false;
        return;
    }

    ShowWindow(g_hWnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hWnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsLight(); 
    
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 12.0f;     
    style.FrameRounding = 6.0f;       
    style.PopupRounding = 8.0f;
    style.WindowBorderSize = 0.0f;    
    style.ChildBorderSize = 0.0f;     
    
    style.Colors[ImGuiCol_WindowBg]         = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_CheckMark]        = ImVec4(0.00f, 0.48f, 1.00f, 1.00f); 
    style.Colors[ImGuiCol_SliderGrab]       = ImVec4(0.00f, 0.48f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_Button]           = ImVec4(0.92f, 0.92f, 0.95f, 1.00f); 
    style.Colors[ImGuiCol_ButtonHovered]    = ImVec4(0.84f, 0.84f, 0.86f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive]     = ImVec4(0.00f, 0.48f, 1.00f, 1.00f);

    ImGuiIO& io = ImGui::GetIO();
    char fontPath[MAX_PATH];
    GetWindowsDirectoryA(fontPath, MAX_PATH);
    strcat_s(fontPath, "\\Fonts\\msjh.ttc"); 
    io.Fonts->AddFontFromFileTTF(fontPath, 18.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull());

    ImGui_ImplWin32_Init(g_hWnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    g_Initialized = true; 

    while (g_Running) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        float screenW = (float)GetSystemMetrics(SM_CXSCREEN);
        float screenH = (float)GetSystemMetrics(SM_CYSCREEN);
        float centerX = screenW / 2.0f;
        float centerY = screenH / 2.0f;

        // ─── 畫布層 ───
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos);
        ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
        ImGui::Begin("Canvas", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
        
        ImDrawList* canvas_draw = ImGui::GetWindowDrawList();

        if (g_AimbotState) {
            canvas_draw->AddCircle(ImVec2(centerX, centerY), g_AimbotFov, IM_COL32(0, 122, 255, 60), 32, 1.5f);
        }
        canvas_draw->AddCircleFilled(ImVec2(centerX, centerY), 3.5f, IM_COL32(255, 69, 58, 255)); 

        {
            std::lock_guard<std::mutex> lock(g_TargetMutex);
            ImU32 box_color_u32 = IM_COL32((int)(g_BoxColor[0]*255), (int)(g_BoxColor[1]*255), (int)(g_BoxColor[2]*255), (int)(g_BoxColor[3]*255));
            
            for (const auto& target : g_DetectedTargets) {
                if (g_EspBox) {
                    canvas_draw->AddRect(
                        ImVec2(target.x - target.w / 2.0f, target.y - target.h / 2.0f),
                        ImVec2(target.x + target.w / 2.0f, target.y + target.h / 2.0f),
                        box_color_u32, 0.0f, 0, 2.0f
                    );
                }
                if (g_EspLine) {
                    canvas_draw->AddLine(
                        ImVec2(centerX, screenH),
                        ImVec2(target.x, target.y + target.h / 2.0f),
                        box_color_u32, 1.5f
                    );
                }
            }
        }
        ImGui::End();

        // ─── 控制選單層 ───
        if (g_ShowMenu) {
            SetWindowLong(g_hWnd, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_LAYERED);
            ImGui::SetNextWindowSize(ImVec2(520, 360), ImGuiCond_FirstUseEver);
            ImGui::Begin(u8"XUANS 高級核心控制器", nullptr, ImGuiWindowFlags_NoTitleBar);
            
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 pos = ImGui::GetWindowPos();
            draw_list->AddCircleFilled(ImVec2(pos.x + 20.0f, pos.y + 22.0f), 6.0f, IM_COL32(255, 95, 86, 255));     
            draw_list->AddCircleFilled(ImVec2(pos.x + 38.0f, pos.y + 22.0f), 6.0f, IM_COL32(255, 189, 46, 255)); 
            draw_list->AddCircleFilled(ImVec2(pos.x + 56.0f, pos.y + 22.0f), 6.0f, IM_COL32(39, 201, 63, 255)); 

            ImGui::SetCursorPos(ImVec2(80.0f, 12.0f));
            ImGui::TextDisabled(u8"XUANS 高級核心控制器");
            ImGui::SetCursorPosY(45.0f); ImGui::Separator();
            
            ImGui::BeginChild("Sidebar", ImVec2(125, 0), false, ImGuiWindowFlags_NoBackground);
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            int pushedColors = 0;

            if (g_CurrentTab == 0) { ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.48f, 1.00f, 1.00f)); ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f)); pushedColors = 2; }
            if (ImGui::Button(u8"主控自瞄", ImVec2(115, 40))) g_CurrentTab = 0;
            if (pushedColors > 0) { ImGui::PopStyleColor(pushedColors); pushedColors = 0; }
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            
            if (g_CurrentTab == 1) { ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.48f, 1.00f, 1.00f)); ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f)); pushedColors = 2; }
            if (ImGui::Button(u8"視覺透視", ImVec2(115, 40))) g_CurrentTab = 1;
            if (pushedColors > 0) { ImGui::PopStyleColor(pushedColors); pushedColors = 0; }
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            
            if (g_CurrentTab == 2) { ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.48f, 1.00f, 1.00f)); ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f)); pushedColors = 2; }
            if (ImGui::Button(u8"系統設置", ImVec2(115, 40))) g_CurrentTab = 2;
            if (pushedColors > 0) { ImGui::PopStyleColor(pushedColors); pushedColors = 0; }
            ImGui::EndChild();
            
            ImGui::SameLine(0.0f, 15.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.90f, 0.90f, 0.92f, 1.00f)); 
            ImGui::BeginChild("ContentBody", ImVec2(0, 0), true, ImGuiWindowFlags_NoBackground);
            ImGui::Dummy(ImVec2(0.0f, 2.0f));
            
            if (g_CurrentTab == 0) {
                ImGui::Text(u8"核心狀態: 正常注入 (FPS: 60)");
                ImGui::Separator(); ImGui::Dummy(ImVec2(0.0f, 5.0f));
                ImGui::Checkbox(u8"啟用 YOLO 視覺自動追蹤", &g_AimbotState);
                if (g_AimbotState) {
                    ImGui::Dummy(ImVec2(0.0f, 5.0f));
                    ImGui::SliderFloat(u8"追蹤範圍 (FOV)", &g_AimbotFov, 30.0f, 300.0f, "%.0f px");
                    ImGui::SliderFloat(u8"滑動平滑度 (Smooth)", &g_AimbotSmooth, 1.0f, 20.0f, "%.1f");
                    ImGui::SetNextItemWidth(180.0f);
                    if (ImGui::BeginCombo(u8"瞄準部位", g_BoneNames[g_TargetBone])) {
                        for (int n = 0; n < 3; n++) {
                            if (ImGui::Selectable(g_BoneNames[n], g_TargetBone == n)) g_TargetBone = n;
                        }
                        ImGui::EndCombo();
                    }
                }
            } 
            else if (g_CurrentTab == 1) {
                ImGui::Text(u8"視覺外觀覆蓋設定");
                ImGui::Separator(); ImGui::Dummy(ImVec2(0.0f, 5.0f));
                ImGui::Checkbox(u8"顯示目標方框 (2D Box)", &g_EspBox);
                ImGui::Checkbox(u8"顯示追蹤射線 (Snaplines)", &g_EspLine);
                if (g_EspBox) {
                    ImGui::Dummy(ImVec2(0.0f, 5.0f)); ImGui::Text(u8"外框顏色自訂:"); ImGui::SameLine();
                    ImGui::ColorEdit4(u8"##BoxColorPicker", g_BoxColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                }
            } 
            else if (g_CurrentTab == 2) {
                ImGui::Text(u8"XS 系統核心資訊");
                ImGui::Separator(); ImGui::Dummy(ImVec2(0.0f, 5.0f));
                ImGui::TextDisabled(u8"授權團隊: XUANS 開發團隊");
                ImGui::TextDisabled(u8"技術核心: C++ Native (D3D11)");
                ImGui::Dummy(ImVec2(0.0f, 15.0f));
                if (ImGui::Button(u8"安全卸載核心", ImVec2(140, 35))) g_Running = false;
            }
            
            ImGui::EndChild();
            ImGui::PopStyleColor(); ImGui::PopStyleVar();   
            ImGui::End();
        } else {
            SetWindowLong(g_hWnd, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED);
        }

        ImGui::Render();
        
        const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0); 
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    if (g_hWnd) { DestroyWindow(g_hWnd); g_hWnd = nullptr; }
    UnregisterClassW(L"XUANS_Overlay", GetModuleHandle(nullptr));
}

// ─── 嚴格的 C 導出介面 ───
extern "C" {
    __declspec(dllexport) void StartOverlay() {
        if (g_Running) return;
        g_Running = true; g_Initialized = false; 
        g_RenderThread = std::thread(RenderLoop);
    }
    __declspec(dllexport) void StopOverlay() { 
        if (!g_Running) return;
        g_Running = false; g_Initialized = false; 
        if (g_RenderThread.joinable()) g_RenderThread.join();
    }
    __declspec(dllexport) void ToggleMenu(bool visible) { if (g_Initialized) g_ShowMenu = visible; }
    __declspec(dllexport) bool GetAimbotState() { return g_AimbotState; }
    __declspec(dllexport) float gGetAimbotFov() { return g_AimbotFov; } 
    __declspec(dllexport) float GetAimbotSmooth() { return g_AimbotSmooth; }
    __declspec(dllexport) int GetTargetBone() { return g_TargetBone; }
    __declspec(dllexport) bool IsOverlayReady() { return g_Initialized.load(); }

    __declspec(dllexport) void UpdateYoloTargets(float* x_arr, float* y_arr, float* w_arr, float* h_arr, int count) {
        if (!g_Initialized) return;
        std::lock_guard<std::mutex> lock(g_TargetMutex);
        g_DetectedTargets.clear();
        for (int i = 0; i < count; i++) {
            g_DetectedTargets.push_back({ x_arr[i], y_arr[i], w_arr[i], h_arr[i] });
        }
    }
}
