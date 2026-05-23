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
bool g_ShowMenu = true;       
bool g_AimbotState = false;   

std::thread g_RenderThread;   

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(g_hWnd);
    ImGui_ImplOpenGL3_Init("#version 130");

    // =================================================================
    // 🎯 核心修復：載入 Windows 內建微軟正黑體，解決問號亂碼
    // =================================================================
    ImGuiIO& io = ImGui::GetIO();
    char fontPath[MAX_PATH];
    GetWindowsDirectoryA(fontPath, MAX_PATH);
    strcat_s(fontPath, "\\Fonts\\msjh.ttc"); // 指向微軟正黑體

    // 載入字體，設定大小為 18 像素，並載入完整的中文繁簡字元集
    ImFont* font = io.Fonts->AddFontFromFileTTF(fontPath, 18.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull());
    if (font == nullptr) {
        // 如果找不到正黑體，防呆改載入新細明體
        GetWindowsDirectoryA(fontPath, MAX_PATH);
        strcat_s(fontPath, "\\Fonts\\mingliu.ttc");
        io.Fonts->AddFontFromFileTTF(fontPath, 18.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull());
    }
    // =================================================================

    g_Running = true;

    while (g_Running) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // 畫布層：繪製中心準心
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos);
        ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
        ImGui::Begin("Canvas", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
        
        float centerX = GetSystemMetrics(SM_CXSCREEN) / 2.0f;
        float centerY = GetSystemMetrics(SM_CYSCREEN) / 2.0f;
        ImGui::GetWindowDrawList()->AddCircle(ImVec2(centerX, centerY), 8.0f, IM_COL32(255, 0, 0, 255), 12, 2.0f);
        
        ImGui::End();

        // 控制選單層
        if (g_ShowMenu) {
            SetWindowLong(g_hWnd, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_LAYERED);
            
            // 開始渲染主控制面板
            ImGui::Begin(u8"XUANS 高級核心控制器", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text(u8"核心狀態: 正常注入 (FPS: 60)");
            ImGui::Separator();
            
            ImGui::Checkbox(u8"啟用 YOLO 視覺自動追蹤", &g_AimbotState);
            
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
        std::cout << "[XUANS Native] 收到啟動訊號，正在開闢獨立執行緒建立渲染視窗..." << std::endl;
        g_RenderThread = std::thread(RenderLoop);
        g_RenderThread.detach();
    }

    __declspec(dllexport) void StopOverlay() {
        if (!g_Running) return;
        std::cout << "[XUANS Native] 收到關閉訊號，正在安全卸載核心..." << std::endl;
        g_Running = false;
    }

    __declspec(dllexport) void ToggleMenu(bool visible) {
        g_ShowMenu = visible;
        std::cout << "[XUANS Native] 外部選單顯示切換為: " << (visible ? "顯示" : "隱藏") << std::endl;
    }

    __declspec(dllexport) bool GetAimbotState() {
        return g_AimbotState;
    }
}
