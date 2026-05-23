#include <windows.h>
#include <dwmapi.h>
#include <GL/gl.h>
#include <iostream>

// 引入 Dear ImGui 核心標頭檔
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"

// 連結 Windows 系統庫
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "opengl32.lib")

// 全域變數
HWND g_hWnd = nullptr;
HDC g_hDC = nullptr;
HGLRC g_hRC = nullptr;
bool g_Running = false;

// 轉發 Win32 視窗訊息給 ImGui
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// 視窗回呼函式
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
        case WM_CLOSE:
            g_Running = false;
            PostQuitMessage(0);
            return 0;
        case WM_DESTROY:
            return 0;
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

// =================================================================
// 匯出給 Python 呼叫的標準接口 (C-Style API)
// =================================================================

extern "C" {

    // 1. 初始化並建立穿透選單視窗
    __declspec(dllexport) bool InitOverlay(const char* windowTitle, int width, int height) {
        std::cout << "[XUANS Native] 正在初始化 ImGui 渲染核心..." << std::endl;

        // 註冊視窗類別
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_HREDRAW | CS_VREDRAW, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"XUANS_Overlay", nullptr };
        RegisterClassExW(&wc);

        // 建立無邊框視窗
        g_hWnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED, // 頂層、滑鼠穿透、分層樣式
            L"XUANS_Overlay", L"XUANS Overlay Menu",
            WS_POPUP, // 無邊框
            100, 100, width, height,
            nullptr, nullptr, wc.hInstance, nullptr
        );

        if (!g_hWnd) return false;

        // 設定透明色標 (實現全視窗穿透背景)
        SetLayeredWindowAttributes(g_hWnd, RGB(0, 0, 0), 255, LWA_COLORKEY);

        // 啟用 DWM 視窗模糊/全黑穿透拓展
        MARGINS margins = { -1 };
        DwmExtendFrameIntoClientArea(g_hWnd, &margins);

        // 初始化 OpenGL 渲染環境
        PIXELFORMATDESCRIPTOR pfd = { sizeof(PIXELFORMATDESCRIPTOR), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, PFD_MAIN_PLANE, 0, 0, 0, 0 };
        g_hDC = GetDC(g_hWnd);
        int pixelFormat = ChoosePixelFormat(g_hDC, &pfd);
        SetPixelFormat(g_hDC, pixelFormat, &pfd);
        g_hRC = wglCreateContext(g_hDC);
        wglMakeCurrent(g_hDC, g_hRC);

        // 顯示視窗
        ShowWindow(g_hWnd, SW_SHOWDEFAULT);
        UpdateWindow(g_hWnd);

        // 初始化 Dear ImGui 上下文
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // 啟用鍵盤導覽

        // 設定 ImGui 視覺主題 (酷炫黑)
        ImGui::StyleColorsDark();

        // 綁定平台與渲染器後台
        ImGui_ImplWin32_Init(g_hWnd);
        ImGui_ImplOpenGL3_Init("#version 130");

        g_Running = true;
        return true;
    }

    // 2. 執行主渲染循環 (Python 通常在一個 Thread 裡死循環呼叫此函式)
    __declspec(dllexport) void RenderFrame() {
        if (!g_Running) return;

        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) g_Running = false;
        }

        // 開始新的一幀
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // ---------------------------------------------------------
        // 這裡就是渲染你硬核介面的地方！
        // ---------------------------------------------------------
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos);
        ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
        
        // 建立一個不帶背景的透明底板視窗畫布
        ImGui::Begin("Canvas", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
        
        // 範例：繪製準心或文字在畫面上 (測試用)
        ImGui::GetWindowDrawList()->AddCircle(ImVec2(960, 540), 10.0f, IM_COL32(255, 0, 0, 255), 12, 2.0f);
        
        ImGui::End();

        // 獨立出的操作控制主選單
        ImGui::Begin("XUANS 高級核心控制器", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("系統狀態: 核心正常注入 (FPS: 60)");
        ImGui::Separator();
        
        static bool aiTrack = false;
        ImGui::Checkbox("啟用 YOLO 視覺自動追蹤", &aiTrack);
        
        static float aimSpeed = 0.5f;
        ImGui::SliderFloat("平滑度 (Smooth)", &aimSpeed, 0.1f, 1.0f, "%.1f");

        if (ImGui::Button("強行清空後台快取")) {
            std::cout << "[C++] 正在清除記憶體特徵與快取..." << std::endl;
        }
        
        ImGui::End();
        // ---------------------------------------------------------

        // 渲染與交換緩衝區
        ImGui::Render();
        glViewport(0, 0, (int)ImGui::GetMainViewport()->Size.x, (int)ImGui::GetMainViewport()->Size.y);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        SwapBuffers(g_hDC);
    }

    // 3. 釋放資源並關閉選單
    __declspec(dllexport) void ShutdownOverlay() {
        std::cout << "[XUANS Native] 正在關閉並銷毀渲染核心..." << std::endl;
        g_Running = false;
        
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(g_hRC);
        ReleaseDC(g_hWnd, g_hDC);
        DestroyWindow(g_hWnd);
        UnregisterClassW(L"XUANS_Overlay", GetModuleHandle(nullptr));
    }

    // 4. 提供給 Python 檢查視窗是否還在運行
    __declspec(dllexport) bool IsRunning() {
        return g_Running;
    }
}
