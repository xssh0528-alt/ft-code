#include <windows.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>

// 指向 imgui 資料夾（改用官方標準後端，避免殘缺手寫導致黑畫面/崩潰）
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

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

    // 🎯 點擊穿透關鍵：全透明背景設置
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
            // 當選單顯示時，移除 WS_EX_TRANSPARENT 以允許滑鼠點擊選單
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
            // 選單關閉時，必須加上 WS_EX_TRANSPARENT 實現極致的「滑鼠全穿透」，才不會卡住遊戲視角
            SetWindowLong(g_hWnd, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED);
        }

        ImGui::Render();
        
        // 🎯 核心修正：將透明背景渲染到後端緩衝區
        const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        
        // 呼叫官方標準後端渲染繪圖指令
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0); 
    }

    // ─── 乾淨的資源銷毀 ───
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    if (g_hWnd) {
        DestroyWindow(g_hWnd);
        g_hWnd = nullptr;
    }
    UnregisterClassW(L"XUANS_Overlay", GetModuleHandle(nullptr));
}

// ─── 嚴格的 C 導出介面 ───
extern "C" {
    __declspec(dllexport) void StartOverlay() {
        if (g_Running) return;
        g_Running = true; 
        g_Initialized = false; 
        g_RenderThread = std::thread(RenderLoop);
    }

    __declspec(dllexport) void StopOverlay() { 
        if (!g_Running) return;
        g_Running = false; 
        g_Initialized = false; 
        if (g_RenderThread.joinable()) {
            g_RenderThread.join(); // 🎯 修正：使用 join 確保線程完全結束才退出 DLL，防範釋放記憶體時閃退
        }
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
