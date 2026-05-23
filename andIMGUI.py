import flet as ft

def main(page: ft.Page):
    # --- 1. 頁面基本配置 ---
    page.title = "XS PRO"
    page.theme_mode = ft.ThemeMode.LIGHT
    page.window.width = 420
    page.window.height = 750
    page.padding = 0
    page.bgcolor = "#F5F7FA" # 輕微的灰色背景讓卡片更明顯

    # --- 2. 邏輯函數 ---
    def on_nav_change(e):
        index = e.control.selected_index
        home_view.visible = (index == 0)
        visuals_view.visible = (index == 1)
        aimbot_view.visible = (index == 2)
        page.update()

    def start_init(e):
        # 彈出底部提示訊息
        page.snack_bar = ft.SnackBar(
            ft.Text("正在連線至伺服器並初始化數據..."),
            action="知道了",
            bgcolor="#0284C7"
        )
        page.snack_bar.open = True
        page.update()

    # --- 3. UI 視圖組件 ---

    # 首頁
    home_view = ft.Column([
        ft.Text("主頁", size=28, weight="bold", color="#1A1C1E"),
        ft.Divider(height=10, color="transparent"),
        ft.Card(
            content=ft.Container(
                content=ft.Column([
                    ft.ListTile(
                        leading=ft.Icon(ft.Icons.VERIFIED_USER, color="#22C55E"),
                        title=ft.Text("授權狀態: 有效"),
                        subtitle=ft.Text("到期時間: 2026-12-31"),
                    ),
                    ft.ListTile(
                        leading=ft.Icon(ft.Icons.MEMORY, color="#0284C7"),
                        title=ft.Text("繪製引擎"),
                        subtitle=ft.Text("Flutter Native Renderer (v3.2.0)"),
                    ),
                    ft.Container(
                        content=ft.Button(
                            "啟動初始化", 
                            icon=ft.Icons.PLAY_ARROW, 
                            on_click=start_init,
                            style=ft.ButtonStyle(shape=ft.RoundedRectangleBorder(radius=8)),
                            width=200
                        ),
                        alignment=ft.MainAxisAlignment.CENTER,
                        padding=10
                    )
                ]),
                padding=10
            )
        )
    ], visible=True)

    # 視覺功能頁面 (加上滾動支援)
    visuals_view = ft.Column([
        ft.Text("視覺功能", size=28, weight="bold", color="#1A1C1E"),
        ft.Card(
            content=ft.Container(
                content=ft.Column([
                    ft.Switch(label="顯示射線"),
                    ft.Switch(label="顯示骨骼"),
                    ft.Switch(label="顯示方框"),
                    ft.Switch(label="顯示血量"),
                    ft.Switch(label="顯示距離"),
                    ft.Switch(label="顯示物品"),
                    ft.Switch(label="方框透視"),
                    ft.Switch(label="方框細節"),
                    ft.Switch(label="雷達掃描"),
                    ft.Switch(label="手持武器"),
                ], spacing=5),
                padding=15
            )
        )
    ], visible=False, scroll=ft.ScrollMode.ADAPTIVE, expand=True)

    # 自瞄配置頁面 (加上滾動支援)
    aimbot_view = ft.Column([
        ft.Text("自瞄配置", size=28, weight="bold", color="#1A1C1E"),
        ft.Card(
            content=ft.Container(
                content=ft.Column([
                    ft.Switch(label="開啟智能漏打自瞄", active_color="#EF4444"),
                    ft.Switch(label="預判跟蹤"),
                    ft.Switch(label="顯示 FOV 範圍"),
                    ft.Switch(label="顯示自瞄點"),
                    
                    ft.Divider(height=20),
                    
                    ft.Text("FOV 範圍 (Pixels)", weight="bold"),
                    ft.Slider(min=0, max=800, value=150, label="{value}px"),
                    
                    ft.Text("自瞄平滑度", weight="bold"),
                    ft.Slider(min=0, max=100, value=30, label="{value}%"),
                    
                    ft.Text("跟蹤速度", weight="bold"),
                    ft.Slider(min=0, max=100, value=10, label="{value}"),
                    
                    ft.Text("預判距離", weight="bold"),
                    ft.Slider(min=0, max=20, value=5, label="{value}m"),
                ], spacing=10),
                padding=15
            )
        )
    ], visible=False, scroll=ft.ScrollMode.ADAPTIVE, expand=True)


    # 底部導航欄
    page.navigation_bar = ft.NavigationBar(
        destinations=[
            ft.NavigationBarDestination(icon=ft.Icons.HOME_OUTLINED, selected_icon=ft.Icons.HOME, label="主頁"),
            ft.NavigationBarDestination(icon=ft.Icons.VISIBILITY_OUTLINED, selected_icon=ft.Icons.VISIBILITY, label="視覺"),
            ft.NavigationBarDestination(icon=ft.Icons.TRACK_CHANGES, label="自瞄"),
        ],
        on_change=on_nav_change
    )

    # 將所有內容加入頁面
    page.add(
        ft.Container(
            content=ft.Column([home_view, visuals_view, aimbot_view]),
            padding=20,
            expand=True
        )
    )

# --- 5. 執行程序 ---
if __name__ == "__main__":
    ft.run(main)