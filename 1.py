import ctypes
import os
import sys
import asyncio  # 引入異步延時
import flet as ft

# 1. 智慧識別環境並載入對應的動態連結庫 (.dll / .so)
def load_native_lib():
    base_dir = os.path.dirname(__file__) if os.path.dirname(__file__) else "."
    
    if hasattr(sys, "getandroiddata") or "ANDROID_BOOTLOGO" in os.environ:
        lib_path = os.path.join(base_dir, "assets", "liboverlay.so")
    else:
        lib_name = "liboverlay.dll" if sys.platform == "win32" else "liboverlay.so"
        lib_path = os.path.join(base_dir, lib_name)
        
    print(f"[*] 嘗試載入動態庫路徑: {lib_path}")
    
    try:
        native_lib = ctypes.CDLL(lib_path)
        
        # 2. 精確定義 C 函式的輸入參數與回傳型態
        native_lib.StartOverlay.argtypes = []
        native_lib.StartOverlay.restype = None
        
        native_lib.StopOverlay.argtypes = []
        native_lib.StopOverlay.restype = None
        
        native_lib.ToggleMenu.argtypes = [ctypes.c_bool]
        native_lib.ToggleMenu.restype = None
        
        native_lib.GetAimbotState.argtypes = []
        native_lib.GetAimbotState.restype = ctypes.c_bool
        
        print("[+] 動態庫接口繫結成功！")
        return native_lib
    except Exception as e:
        print(f"[-] 載入動態庫失敗: {e}")
        return None

# 初始化執行庫載入
overlay_lib = load_native_lib()

# 將主函式也改為 async 確保異步流暢
async def main(page: ft.Page):
    page.title = "XS AI 核心管理器"
    page.theme_mode = ft.ThemeMode.DARK
    page.window_width = 400
    page.window_height = 450
    page.vertical_alignment = ft.MainAxisAlignment.CENTER
    page.horizontal_alignment = ft.CrossAxisAlignment.CENTER
    
    menu_visible = True
    
    status_text = ft.Text("引擎狀態：未啟動", color=ft.Colors.RED_ACCENT, weight=ft.FontWeight.BOLD, size=16)
    cb_status = ft.Text("自瞄狀態：等待同步...", size=14, color=ft.Colors.CYAN_200)

    # 3. 修正為 async 協程函式：定時從 C++ 內部把選單數據同步回 Flet 介面
    async def sync_data_task():
        while True:
            if overlay_lib:
                try:
                    c_aim_state = overlay_lib.GetAimbotState()
                    cb_status.value = f"底層自瞄即時狀態: {'【 鎖定中 】' if c_aim_state else '【 未開啟 】'}"
                    await page.update_async() if hasattr(page, "update_async") else page.update()
                except Exception as ex:
                    print(f"數據同步異常: {ex}")
            await asyncio.sleep(0.4) # 使用異步非阻塞睡眠

    # 4. 控制台開關事件
    def on_engine_switch(e):
        if not overlay_lib:
            switch_btn.value = False
            page.update()
            return
            
        if switch_btn.value:
            overlay_lib.StartOverlay()
            status_text.value = "引擎狀態：運作中 (渲染視窗已就緒)"
            status_text.color = ft.Colors.GREEN_ACCENT
            # 新版 Flet 要求的異步任務啟動方式
            page.run_task(sync_data_task)
        else:
            overlay_lib.StopOverlay()
            status_text.value = "引擎狀態：已安全關閉"
            status_text.color = ft.Colors.RED_ACCENT
        page.update()

    # 5. 強制切換隱藏/顯示 C++ 外部選單
    def toggle_imgui_menu(e):
        nonlocal menu_visible
        if not overlay_lib: return
        menu_visible = not menu_visible
        overlay_lib.ToggleMenu(menu_visible)
        toggle_btn.text = "隱藏選單" if menu_visible else "呼叫選單"
        page.update()

    # 安全機制：當 Flet App 被打叉關閉時，強制終止底層 C++ 線程
    def on_app_close(e):
        if overlay_lib:
            try:
                overlay_lib.StopOverlay()
                print("[+] 檢測到 App 關閉，已安全停止 C++ 線程。")
            except:
                pass
    page.on_close = on_app_close

    # UI 佈局元件定義 (全面採用符合 Flet 1.0 的全標準新語法)
    switch_btn = ft.Switch(label="啟動 C++ 渲染後台", on_change=on_engine_switch)
    toggle_btn = ft.FilledButton("隱藏選單", on_click=toggle_imgui_menu, icon=ft.Icons.DISPLAY_SETTINGS)

    # 建構控制台 UI
    page.add(
        ft.Card(
            content=ft.Container(
                content=ft.Column([
                    ft.Text("XS PRO CONTROL", size=22, weight=ft.FontWeight.BOLD, color=ft.Colors.BLUE_400),
                    ft.Divider(color=ft.Colors.BLUE_GREY_800),
                    status_text,
                    switch_btn,
                    ft.Divider(height=15, color=ft.Colors.TRANSPARENT),
                    toggle_btn,
                    ft.Container(
                        content=cb_status,
                        padding=12,
                        bgcolor=ft.Colors.BLUE_GREY_900,
                        border_radius=8,
                        margin=ft.Margin(0, 15, 0, 0) # <-- 修正這裡：完全移除 ft.margin 警告
                    )
                ], horizontal_alignment=ft.CrossAxisAlignment.CENTER),
                padding=25,
                width=340
            )
        )
    )

# 執行主程式
ft.run(main)