#include <windows.h>
#include <dwmapi.h>
#include <GL/gl.h>
#include <iostream>
#include <thread>

// 引入 Dear ImGui 核心標頭檔
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"

// 連結 Windows 系統庫
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "opengl32.lib")

// 全域控制變數
HWND g_hWnd = nullptr;
HDC g_hDC = nullptr;
HGLRC g_hRC = nullptr;
bool g_Running = false;
bool g_ShowMenu = true;       // 對應 Python 的 ToggleMenu
bool g_AimbotState = false;   // 測試用的自瞄狀態變數

std::thread g_RenderThread;   // 渲染專用獨立執行緒

// 轉發 Win32 視窗訊息給 ImGui
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// 視窗回呼函式
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
        case WM_CLOSE:
            g_Running = false;
            return 0;
        case WM_DESTROY:
            return 0;
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

// 實際在獨立執行緒運作的渲染循環
void RenderLoop() {
    // 註冊視窗類別
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_HREDRAW | CS_VREDRAW, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"XUANS_Overlay", nullptr };
    RegisterClassExW(&wc);

    // 建立透明穿透選單視窗 (預設 1920x1080 全螢幕覆蓋)
    g_hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED,
        L"XUANS_Overlay", L"XUANS Overlay Menu",
        WS_POPUP,
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        nullptr, nullptr, wc.hInstance, nullptr
    );

    if (!g_hWnd) return;

    // 設定背景全透明
    SetLayeredWindowAttributes(g_hWnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
    MARGINS margins = { -1 };
    DwmExtendFrameIntoClientArea(g_hWnd, &margins);

    // 初始化 OpenGL 渲染
    PIXELFORMATDESCRIPTOR pfd = { sizeof(PIXELFORMATDESCRIPTOR), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, PFD_MAIN_PLANE, 0, 0, 0, 0 };
    g_hDC = GetDC(g_hWnd);
    int pixelFormat = ChoosePixelFormat(g_hDC, &pfd);
    SetPixelFormat(g_hDC, pixelFormat, &pfd);
    g_hRC = wglCreateContext(g_hDC);
    wglMakeCurrent(g_hDC, g_hRC);

    ShowWindow(g_hWnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hWnd);

    // 初始化 ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(g_hWnd);
    ImGui_ImplOpenGL3_Init("#version 130");

    g_Running = true;

    // 主渲染死循環
    while (g_Running) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // 開始繪製新的一幀
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // 畫布層 (就算選單隱藏，這層依然可以畫透視框/準心)
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos);
        ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
        ImGui::Begin("Canvas", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
        
        // 畫一個紅色的測試中心準心
        float centerX = GetSystemMetrics(SM_CXSCREEN) / 2.0f;
        float centerY = GetSystemMetrics(SM_CYSCREEN) / 2.0f;
        ImGui::GetWindowDrawList()->AddCircle(ImVec2(centerX, centerY), 8.0f, IM_COL32(255, 0, 0, 255), 12, 2.0f);
        
        ImGui::End();

        // 只有在 g_ShowMenu 為 true 時才渲染 ImGui 主調試選單
        if (g_ShowMenu) {
            // 讓滑鼠可以點擊到選單上 (取消穿透)
            SetWindowLong(g_hWnd, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_LAYERED);

            ImGui::Begin("XUANS 高級核心控制器", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("核心狀態: 正常注入 (FPS: 60)");
            ImGui::Separator();
            
            // 這裡改變狀態，Python 的 sync_data_task 會自動同步抓到！
            ImGui::Checkbox("啟用 YOLO 視覺自動追蹤", &g_AimbotState);
            
            ImGui::End();
        } else {
            // 隱藏選單時，開啟滑鼠穿透狀態 (完全不卡滑鼠操作)
            SetWindowLong(g_hWnd, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED);
        }

        // 渲染呈現
        ImGui::Render();
        glViewport(0, 0, (int)ImGui::GetMainViewport()->Size.x, (int)ImGui::GetMainViewport()->Size.y);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        SwapBuffers(g_hDC);
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // 限制 FPS 在 60 左右
    }

    // 釋放資源
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(g_hRC);
    ReleaseDC(g_hWnd, g_hDC);
    DestroyWindow(g_hWnd);
    UnregisterClassW(L"XUANS_Overlay", GetModuleHandle(nullptr));
}

// =================================================================
// 🚀 完美對齊 Python ctypes 期待的匯出接口
// =================================================================

extern "C" {

    // 1. 對應 Python 的 overlay_lib.StartOverlay()
    __declspec(dllexport) void StartOverlay() {
        if (g_Running) return;
        std::cout << "[XUANS Native] 收到啟動訊號，正在開闢獨立執行緒建立渲染視窗..." << std::endl;
        g_RenderThread = std::thread(RenderLoop);
        g_RenderThread.detach(); // 放行獨立運作
    }

    // 2. 對應 Python 的 overlay_lib.StopOverlay()
    __declspec(dllexport) void StopOverlay() {
        if (!g_Running) return;
        std::cout << "[XUANS Native] 收到關閉訊號，正在安全卸載核心..." << std::endl;
        g_Running = false;
    }

    // 3. 對應 Python 的 overlay_lib.ToggleMenu(menu_visible)
    __declspec(dllexport) void ToggleMenu(bool visible) {
        g_ShowMenu = visible;
        std::cout << "[XUANS Native] 外部選單顯示切換為: " << (visible ? "顯示" : "隱藏") << std::endl;
    }

    // 4. 對應 Python 的 overlay_lib.GetAimbotState()
    __declspec(dllexport) bool GetAimbotState() {
        return g_AimbotState;
    }
}
