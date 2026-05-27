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

// 新增一些測試用的變數，讓分頁內容更豐富
bool g_EspBox = false;
bool g_EspLine = false;
float g_AimbotFov = 90.0f;

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
    
    ImGui::StyleColorsLight(); // 保持亮色主題
    
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 12.0f;     
    style.FrameRounding = 6.0f;       
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 10.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;         // ✨ 新增：頁籤圓角
    style.WindowBorderSize = 0.0f;    
    
    style.Colors[ImGuiCol_TitleBg]          = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive]    = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    style.Colors[ImGuiCol_WindowBg]         = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_CheckMark]        = ImVec4(0.00f, 0.48f, 1.00f, 1.00f); // 蘋果藍
    style.Colors[ImGuiCol_SliderGrab]       = ImVec4(0.00f, 0.48f, 1.00f, 1.00f);
    
    // ✨ 調整頁籤選中與未選中的果味配色
    style.Colors[ImGuiCol_Tab]              = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);
    style.Colors[ImGuiCol_TabHovered]       = ImVec4(0.84f, 0.84f, 0.86f, 1.00f);
    style.Colors[ImGuiCol_TabActive]        = ImVec4(0.00f, 0.48f, 1.00f, 1.00f); // 選中時變成蘋果藍
    style.Colors[ImGuiCol_TabUnfocused]     = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);

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

        // 畫布層 (繪製中心準心)
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos);
        ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
        ImGui::Begin("Canvas", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
        float centerX = GetSystemMetrics(SM_CXSCREEN) / 2.0f;
        float centerY = GetSystemMetrics(SM_CYSCREEN) / 2.0f;
        
        // 如果自瞄開啟，準心外圍多畫一個 FOV 圈圈
        if (g_AimbotState) {
            ImGui::GetWindowDrawList()->AddCircle(ImVec2(centerX, centerY), g_AimbotFov, IM_COL32(0, 122, 255, 100), 32, 1.0f);
        }
        ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(centerX, centerY), 4.0f, IM_COL32(255, 69, 58, 255)); 
        ImGui::End();

        // 控制選單層
        if (g_ShowMenu) {
            SetWindowLong(g_hWnd, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_LAYERED);
            
            // 為了不讓內容太擠，我們稍微設定一下選單的固定寬度
            ImGui::SetNextWindowSize(ImVec2(420, 280), ImGuiCond_FirstUseEver);
            ImGui::Begin(u8"XUANS 高級核心控制器", nullptr, ImGuiWindowFlags_NoTitleBar);
            
            // 手動繪製蘋果紅綠燈
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
            
            ImGui::SetCursorPosY(40.0f); 
            ImGui::Separator();
            
            // 🎯 ✨ 核心亮點：開始分頁欄組件
            if (ImGui::BeginTabBar("XUANS_TabBar", ImGuiTabBarFlags_None)) {
                
                // ─── 第一頁：核心自瞄 ───
                if (ImGui::BeginTabItem(u8"主控自瞄")) {
                    ImGui::Dummy(ImVec2(0.0f, 10.0f)); // 空白間距
                    ImGui::Text(u8"核心狀態: 正常注入 (FPS: 60)");
                    ImGui::Separator();
                    
                    ImGui::Checkbox(u8"啟用 YOLO 視覺自動追蹤", &g_AimbotState);
                    
                    // 只有在開啟自瞄時才顯示 FOV 滑桿
                    if (g_AimbotState) {
                        ImGui::SliderFloat(u8"追蹤範圍 (FOV)", &g_AimbotFov, 30.0f, 300.0f, "%.0f px");
                    }
                    
                    ImGui::EndTabItem();
                }
                
                // ─── 第二頁：視覺透視 ───
                if (ImGui::BeginTabItem(u8"視覺透視")) {
                    ImGui::Dummy(ImVec2(0.0f, 10.0f));
                    ImGui::Checkbox(u8"顯示目標方框 (2D Box)", &g_EspBox);
                    ImGui::Checkbox(u8"顯示追蹤射線 (Snaplines)", &g_EspLine);
                    
                    ImGui::EndTabItem();
                }
                
                // ─── 第三頁：系統設置 ───
                if (ImGui::BeginTabItem(u8"系統設置")) {
                    ImGui::Dummy(ImVec2(0.0f, 10.0f));
                    ImGui::TextDisabled(u8"授權團隊: XUANS 開發團隊");
                    ImGui::TextDisabled(u8"技術核心: Python 3.10 + C++ Native (OpenGL3)");
                    
                    if (ImGui::Button(u8"安全卸載核心")) {
                        g_Running = false;
                    }
                    
                    ImGui::EndTabItem();
                }
                
                ImGui::EndTabBar(); // 結束分頁欄
            }
            
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

extern "C" {
    __declspec(dllexport) void StartOverlay() {
        if (g_Running) return;
        g_RenderThread = std::thread(RenderLoop);
        g_RenderThread.detach();
    }
    __declspec(dllexport) void StopOverlay() { if (g_Running) g_Running = false; }
    __declspec(dllexport) void ToggleMenu(bool visible) { g_ShowMenu = visible; }
    __declspec(dllexport) bool GetAimbotState() { return g_AimbotState; }
}
