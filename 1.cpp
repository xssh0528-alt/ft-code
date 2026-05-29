#include <windows.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>

// 指向 imgui 資料夾
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "d3d11.lib")

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

// ─── 核心魔改：直接在本地實現 ImGui DX11 後端函數 ───
struct ImGui_ImplDX11_Data {
    ID3D11Device* pd3dDevice;
    ID3D11DeviceContext* pd3dDeviceContext;
    IDXGIFactory* pFactory;
    ID3D11Buffer* pVB;
    ID3D11Buffer* pIB;
    ID3D11VertexShader* pVertexShader;
    ID3D11InputLayout* pInputLayout;
    ID3D11Buffer* pVertexConstantBuffer;
    ID3D11PixelShader* pPixelShader;
    ID3D11SamplerState* pFontSampler;
    ID3D11ShaderResourceView*pFontTextureView;
    ID3D11RasterizerState* pRasterizerState;
    ID3D11BlendState* pBlendState;
    ID3D11DepthStencilState*pDepthStencilState;
    int                     VertexBufferSize;
    int                     IndexBufferSize;
    ImGui_ImplDX11_Data()   { memset(this, 0, sizeof(*this)); VertexBufferSize = 5000; IndexBufferSize = 10000; }
};

static ImGui_ImplDX11_Data* ImGui_ImplDX11_GetBackendData() {
    return ImGui::GetCurrentContext() ? (ImGui_ImplDX11_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

IMGUI_IMPL_API bool ImGui_ImplDX11_Init(ID3D11Device* device, ID3D11DeviceContext* device_context) {
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Renderer backend already initialized!");
    ImGui_ImplDX11_Data* bd = IM_NEW(ImGui_ImplDX11_Data)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_dx11_xuans";
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
    if (bd->pFontTextureView) { 
        bd->pFontTextureView->Release(); 
        bd->pFontTextureView = nullptr; 
        ImGuiIO& io = ImGui::GetIO(); // 🎯 修正點 1：補上 io 宣告，解決 C2065
        io.Fonts->SetTexID(nullptr); 
    }
    if (bd->pRasterizerState) { bd->pRasterizerState->Release(); bd->pRasterizerState = nullptr; }
    if (bd->pBlendState) { bd->pBlendState->Release(); bd->pBlendState = nullptr; }
    if (bd->pDepthStencilState) { bd->pDepthStencilState->Release(); bd->pDepthStencilState = nullptr; }
}

IMGUI_IMPL_API void ImGui_ImplDX11_Shutdown() {
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    if (!bd) return;
    ImGui_ImplDX11_DestroyDeviceObjects();
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererUserData = nullptr;
    io.BackendRendererName = nullptr;
    IM_DELETE(bd);
}

static void ImGui_ImplDX11_CreateDeviceObjects() {
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    if (!bd || bd->pFontTextureView) return;

    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels; int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = width; desc.Height = height; desc.MipLevels = 1; desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = pixels; subResource.SysMemPitch = width * 4; subResource.SysMemSlicePitch = 0;
    ID3D11Texture2D* pTexture = nullptr;
    bd->pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    bd->pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, &bd->pFontTextureView);
    pTexture->Release();
    io.Fonts->SetTexID((ImTextureID)bd->pFontTextureView);

    D3D11_BLEND_DESC blend_desc; ZeroMemory(&blend_desc, sizeof(blend_desc));
    blend_desc.AlphaToCoverageEnable = FALSE; blend_desc.RenderTarget[0].BlendEnable = TRUE;
    blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD; blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA; blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    bd->pd3dDevice->CreateBlendState(&blend_desc, &bd->pBlendState);

    D3D11_DEPTH_STENCIL_DESC depth_desc; ZeroMemory(&depth_desc, sizeof(depth_desc));
    depth_desc.DepthEnable = FALSE; depth_desc.StencilEnable = FALSE;
    bd->pd3dDevice->CreateDepthStencilState(&depth_desc, &bd->pDepthStencilState);
}

IMGUI_IMPL_API void ImGui_ImplDX11_NewFrame() {
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    if (!bd) return;
    if (!bd->pFontTextureView) ImGui_ImplDX11_CreateDeviceObjects();
}

IMGUI_IMPL_API void ImGui_ImplDX11_RenderDrawData(ImDrawData* draw_data) {
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    if (!bd || draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f) return;

    bd->pd3dDeviceContext->OMSetBlendState(bd->pBlendState, nullptr, 0xffffffff);
    bd->pd3dDeviceContext->OMSetDepthStencilState(bd->pDepthStencilState, 0);
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
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_
