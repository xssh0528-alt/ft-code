#include <windows.h>
#include <dwmapi.h>
#include <GL/gl.h>
#include <iostream>
#include <thread>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "opengl32.lib")

HWND g_hWnd = nullptr;
HDC g_hDC = nullptr;
HGLRC g_hRC = nullptr;
bool g_Running = false;
bool g_ShowMenu = true;       
bool g_AimbotState = false;   

// ─── 舊有與全新功能變數 ───
bool g_EspBox = false;
bool g_EspLine = false;
float g_AimbotFov = 90.0f;

// ✨ 新功能 1：平滑度參數 (預設 5.0f，數值越高越平緩防封)
float g_AimbotSmooth = 5.0f; 

// ✨ 新功能 2：鎖定部位 (0: 頭部, 1: 胸口, 2: 腹部)
int g_TargetBone = 0; 
const char* g_BoneNames[] = { u8"骨骼: 頭部 (Head)", u8"骨骼: 胸口 (Chest)", u8"骨骼: 腹部 (Pelvis)" };

// ✨ 新功能 3：透視方框顏色 (RGBA 陣列，預設為蘋果經典藍色)
float g_BoxColor[4] = { 0.0f, 0.48f, 1.0f, 1.0f }; 

// 記錄當前切換到哪一個分頁 (0: 主控, 1: 視覺, 2: 設置)
int g_CurrentTab = 0; 

std::thread g_RenderThread;   

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg) {
        case WM_CLOSE: g_Running = false; return 0;
        case WM_DESTROY: return 0;
        default: return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

void RenderLoop() {
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_HREDRAW | CS_VREDRAW, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"XUANS_Overlay", nullptr };
    RegisterClassExW(&wc);

    g_hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED,
        L"XUANS_Overlay", L"XUANS Overlay Menu",
        WS_POPUP,
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        nullptr, nullptr, wc.hInstance, nullptr
    );

    if (!g_hWnd) return;

    SetLayeredWindowAttributes(g_hWnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
    MARGINS margins = { -1 };
    DwmExtendFrameIntoClientArea(g_hWnd, &margins);

    PIXELFORMATDESCRIPTOR pfd = { sizeof(PIXELFORMATDESCRIPTOR), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, PFD_MAIN_PLANE, 0, 0, 0, 0 };
    g_hDC = GetDC(g_hWnd);
    int pixelFormat = ChoosePixelFormat(g_hDC, &pfd);
    SetPixelFormat(g_hDC, pixelFormat, &pfd);
    g_hRC = wglCreateContext(g_hDC);
    
    wglMakeCurrent(g_hDC, g_hRC);

    ShowWindow(g_hWnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hWnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsLight(); 
    
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 12.0f;     
    style.FrameRounding = 6.0f;       
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 10.0f;
    style.GrabRounding = 6.0f;
    style.ChildRounding = 8.0f;       
    style.WindowBorderSize = 0.0f;    
    style.ChildBorderSize = 0.0f;     
    
    style.Colors[ImGuiCol_TitleBg]          = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive]    = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
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
    ImFont* font = io.Fonts->AddFontFromFileTTF(fontPath, 18.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull());
    if (font == nullptr) {
        GetWindowsDirectoryA(fontPath, MAX_PATH);
        strcat_s(fontPath, "\\Fonts\\mingliu.ttc");
        io.Fonts->AddFontFromFileTTF(fontPath, 18.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull());
    }

    ImGui_ImplWin32_Init(g_hWnd);
    ImGui_ImplOpenGL3_Init("#version 130");

    g_Running = true;

    while (g_Running) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        wglMakeCurrent(g_hDC, g_hRC);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // 畫布層 (繪製中心準心與動態 FOV)
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos);
        ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
        ImGui::Begin("Canvas", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
        float centerX = GetSystemMetrics(SM_CXSCREEN) / 2.0f;
        float centerY = GetSystemMetrics(SM_CYSCREEN) / 2.0f;
        
        if (g_AimbotState) {
            // 使用動態調整的 FOV 圓圈
            ImGui::GetWindowDrawList()->AddCircle(ImVec2(centerX, centerY), g_AimbotFov, IM_COL32(0, 122, 255, 100), 32, 1.0f);
        }
        // 中心固定紅點
        ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(centerX, centerY), 4.0f, IM_COL32(255, 69, 58, 255)); 
        ImGui::End();

        // 控制選單層
        if (g_ShowMenu) {
            SetWindowLong(g_hWnd, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_LAYERED);
            
            // 隨著功能變多，稍微加高視窗到 360
            ImGui::SetNextWindowSize(ImVec2(520, 360), ImGuiCond_FirstUseEver);
            ImGui::Begin(u8"XUANS 高級核心控制器", nullptr, ImGuiWindowFlags_NoTitleBar);
            
            // 蘋果紅綠燈
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 pos = ImGui::GetWindowPos();
            float radius = 6.0f;
            float startX = pos.x + 20.0f;
            float startY = pos.y + 22.0f;
            float spacing = 18.0f;

            draw_list->AddCircleFilled(ImVec2(startX, startY), radius, IM_COL32(255, 95, 86, 255));     
            draw_list->AddCircleFilled(ImVec2(startX + spacing, startY), radius, IM_COL32(255, 189, 46, 255)); 
            draw_list->AddCircleFilled(ImVec2(startX + spacing * 2, startY), radius, IM_COL32(39, 201, 63, 255)); 

            ImGui::SetCursorPos(ImVec2(80.0f, 12.0f));
            ImGui::TextDisabled(u8"XUANS 高級核心控制器");
            
            ImGui::SetCursorPosY(45.0f); 
            ImGui::Separator();
            
            // ─── 左側邊欄導航 ───
            ImGui::BeginChild("Sidebar", ImVec2(125, 0), false, ImGuiWindowFlags_NoBackground);
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            
            int pushedColors = 0;

            // 頁籤 1
            if (g_CurrentTab == 0) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.48f, 1.00f, 1.00f)); 
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f));
                pushedColors = 2;
            }
            if (ImGui::Button(u8"主控自瞄", ImVec2(115, 40))) g_CurrentTab = 0;
            if (pushedColors > 0) { ImGui::PopStyleColor(pushedColors); pushedColors = 0; }
            
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            
            // 頁籤 2
            if (g_CurrentTab == 1) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.48f, 1.00f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f));
                pushedColors = 2;
            }
            if (ImGui::Button(u8"視覺透視", ImVec2(115, 40))) g_CurrentTab = 1;
            if (pushedColors > 0) { ImGui::PopStyleColor(pushedColors); pushedColors = 0; }
            
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            
            // 頁籤 3
            if (g_CurrentTab == 2) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.48f, 1.00f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f));
                pushedColors = 2;
            }
            if (ImGui::Button(u8"系統設置", ImVec2(115, 40))) g_CurrentTab = 2;
            if (pushedColors > 0) { ImGui::PopStyleColor(pushedColors); pushedColors = 0; }
            
            ImGui::EndChild();
            
            ImGui::SameLine(0.0f, 15.0f);
            
            // ─── 右側主要內容主體 ───
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.90f, 0.90f, 0.92f, 1.00f)); 
            
            ImGui::BeginChild("ContentBody", ImVec2(0, 0), true, ImGuiWindowFlags_NoBackground);
            ImGui::Dummy(ImVec2(0.0f, 2.0f));
            
            // ─── 分頁 1 內容：自瞄進階控制 ───
            if (g_CurrentTab == 0) {
                ImGui::Text(u8"核心狀態: 正常注入 (FPS: 60)");
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0.0f, 5.0f));
                
                ImGui::Checkbox(u8"啟用 YOLO 視覺自動追蹤", &g_AimbotState);
                
                if (g_AimbotState) {
                    ImGui::Dummy(ImVec2(0.0f, 5.0f));
                    ImGui::SliderFloat(u8"追蹤範圍 (FOV)", &g_AimbotFov, 30.0f, 300.0f, "%.0f px");
                    
                    // ✨ 新增 UI：平滑度滑桿
                    ImGui::SliderFloat(u8"滑動平滑度 (Smooth)", &g_AimbotSmooth, 1.0f, 20.0f, "%.1f");
                    
                    // ✨ 新增 UI：部位選擇下拉選單
                    ImGui::SetNextItemWidth(180.0f);
                    if (ImGui::BeginCombo(u8"瞄準部位", g_BoneNames[g_TargetBone])) {
                        for (int n = 0; n < 3; n++) {
                            bool is_selected = (g_TargetBone == n);
                            if (ImGui::Selectable(g_BoneNames[n], is_selected))
                                g_TargetBone = n;
                            if (is_selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
            } 
            // ─── 分頁 2 內容：視覺與調色盤 ───
            else if (g_CurrentTab == 1) {
                ImGui::Text(u8"視覺外觀覆蓋設定");
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0.0f, 5.0f));
                
                ImGui::Checkbox(u8"顯示目標方框 (2D Box)", &g_EspBox);
                ImGui::Checkbox(u8"顯示追蹤射線 (Snaplines)", &g_EspLine);
                
                // ✨ 新增 UI：如果開啟方框透視，動態展現果味調色盤
                if (g_EspBox) {
                    ImGui::Dummy(ImVec2(0.0f, 5.0f));
                    ImGui::Text(u8"外框顏色自訂:");
                    ImGui::SameLine();
                    ImGui::ColorEdit4(u8"##BoxColorPicker", g_BoxColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                }
            } 
            // ─── 分頁 3 內容：系統設置 ───
            else if (g_CurrentTab == 2) {
                ImGui::Text(u8"XS 系統核心資訊");
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0.0f, 5.0f));
                ImGui::TextDisabled(u8"授權團隊: XUANS 開發團隊");
                ImGui::TextDisabled(u8"技術核心: C++ Native (OpenGL3)");
                ImGui::Dummy(ImVec2(0.0f, 15.0f));
                if (ImGui::Button(u8"安全卸載核心", ImVec2(140, 35))) {
                    g_Running = false;
                }
            }
            
            ImGui::EndChild();
            ImGui::PopStyleColor(); 
            ImGui::PopStyleVar();   
            
            ImGui::End();
        } else {
            SetWindowLong(g_hWnd, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED);
        }

        ImGui::Render();
        glViewport(0, 0, (int)ImGui::GetMainViewport()->Size.x, (int)ImGui::GetMainViewport()->Size.y);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        SwapBuffers(g_hDC);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(g_hRC);
    ReleaseDC(g_hWnd, g_hDC);
    DestroyWindow(g_hWnd);
    UnregisterClassW(L"XUANS_Overlay", GetModuleHandle(nullptr));
}

// ─── 導出 C 介面函數給 Python 連接 ───
extern "C" {
    __declspec(dllexport) void StartOverlay() {
        if (g_Running) return;
        g_RenderThread = std::thread(RenderLoop);
        g_RenderThread.detach();
    }
    __declspec(dllexport) void StopOverlay() { if (g_Running) g_Running = false; }
    __declspec(dllexport) void ToggleMenu(bool visible) { g_ShowMenu = visible; }
    __declspec(dllexport) bool GetAimbotState() { return g_AimbotState; }

    // ✨ 新增導出接口：以便 Python 未來需要直接獲取平滑度和鎖定骨骼數據
    __declspec(dllexport) float GetAimbotSmooth() { return g_AimbotSmooth; }
    __declspec(dllexport) int GetTargetBone() { return g_TargetBone; }
}
