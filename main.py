import flet as ft
import random
import string
import traceback
from supabase import create_client, Client

# 主題顏色配置
BG_COLOR = "#121214"
SURFACE_COLOR = "#1E1E22"
ACCENT_BLUE = "#0288D1"

# ==========================================
# 🌐 1. Supabase 後端初始化設定
# ==========================================
SUPABASE_URL = "https://pdmxkspwnxorsoyowtgx.supabase.co"  # 請保持您原本正確的網址
SUPABASE_KEY = "sb_publishable_ER9oM42esp_34cD-BXDOLA_VAphDItI"  # 請保持您原本正確的金鑰
supabase: Client = create_client(SUPABASE_URL, SUPABASE_KEY)

def main(page: ft.Page):
    # 視窗基礎設定
    page.title = "XS AI KEY MANAGER (完全體旗艦版 - 排版優化)"
    page.window_width = 1450  
    page.window_height = 820
    page.theme_mode = ft.ThemeMode.DARK  
    page.padding = 20
    page.bgcolor = BG_COLOR

    # 全域記憶變數
    selected_key = None
    all_fetched_data = [] 

    # ==========================================
    # ⚡ 2. 核心業務邏輯
    # ==========================================

    # 【功能 A】從 Supabase 讀取最新數據並刷新表格
    def load_data(e=None):
        nonlocal selected_key, all_fetched_data
        selected_key = None
        lbl_status.value = "⏳ 正在拉取雲端數據..."
        lbl_status.color = ft.Colors.AMBER
        page.update()
        
        try:
            response = supabase.table("keys").select("*").order("created_at", desc=True).execute()
            all_fetched_data = response.data
            
            # 更新搜尋與渲染
            render_table_rows(all_fetched_data)
            # 更新數據統計
            update_statistics(all_fetched_data)
                
            lbl_status.value = "● ⚖️ 雲端數據同步成功"
            lbl_status.color = ft.Colors.GREEN_ACCENT
        except Exception as ex:
            lbl_status.value = f"● ❌ 同步失敗: {str(ex)}"
            lbl_status.color = ft.Colors.RED_ACCENT
            print(traceback.format_exc())
        
        page.update()

    # 【輔助功能】按鈕與欄位數據渲染 (修正疊字、強化排版)
    def render_table_rows(data_list):
        data_table.rows.clear()
        for row in data_list:
            key_code = row.get("key_code", "")
            created_at = row.get("created_at", "")
            max_usage = row.get("max_usage", 1)
            used_count = row.get("used_count", 0)
            valid_days = row.get("valid_days", 30)
            
            hwid = row.get("hwid") if row.get("hwid") else "──"
            expires_at = row.get("expires_at") if row.get("expires_at") else "──"
            last_ip = row.get("last_ip") if row.get("last_ip") else "──"
            last_login = row.get("last_login") if row.get("last_login") else "──"
            
            # 清理時間字串格式
            if last_login != "──" and "T" in last_login:
                last_login = last_login.replace("T", " ").split(".")[0]
            if expires_at != "──" and "T" in expires_at:
                expires_at = expires_at.replace("T", " ").split(".")[0]
            if created_at and "T" in created_at:
                clean_created_at = created_at.replace("T", " ").split(".")[0]
            else:
                clean_created_at = created_at
            
            if used_count >= max_usage:
                status_str = f"已使用 ({used_count}/{max_usage})"
                status_color = ft.Colors.RED_ACCENT
            else:
                status_str = f"未使用 ({used_count}/{max_usage})"
                status_color = ft.Colors.GREEN_ACCENT
            
            new_row = ft.DataRow(
                cells=[
                    ft.DataCell(
                        ft.Text(key_code, weight=ft.FontWeight.BOLD, selectable=True),
                        on_tap=lambda e, k=key_code: row_selected(k)
                    ),
                    ft.DataCell(
                        ft.Text(status_str, color=status_color),
                        on_tap=lambda e, k=key_code: row_selected(k)
                    ),
                    ft.DataCell(
                        ft.Text(f"{valid_days} 天", color=ft.Colors.CYAN_400),
                        on_tap=lambda e, k=key_code: row_selected(k)
                    ),
                    ft.DataCell(
                        ft.Text(hwid, color=ft.Colors.PURPLE_200 if hwid != "──" else ft.Colors.GREY_500, size=12, selectable=True),
                        on_tap=lambda e, k=key_code: row_selected(k)
                    ),
                    ft.DataCell(
                        ft.Text(last_ip, color=ft.Colors.ORANGE_300 if last_ip != "──" else ft.Colors.GREY_500),
                        on_tap=lambda e, k=key_code: row_selected(k)
                    ),
                    ft.DataCell(
                        ft.Text(last_login, color=ft.Colors.PINK_300 if last_login != "──" else ft.Colors.GREY_500),
                        on_tap=lambda e, k=key_code: row_selected(k)
                    ),
                    ft.DataCell(
                        ft.Text(expires_at, color=ft.Colors.AMBER_300 if expires_at != "──" else ft.Colors.GREY_500),
                        on_tap=lambda e, k=key_code: row_selected(k)
                    ),
                    ft.DataCell(
                        ft.Text(clean_created_at, color=ft.Colors.GREY_400),
                        on_tap=lambda e, k=key_code: row_selected(k)
                    ),
                ]
            )
            data_table.rows.append(new_row)
        page.update()

    # 【功能 B】本地即時搜尋過濾
    def filter_search(e):
        search_keyword = tf_search.value.strip().upper()
        if not search_keyword:
            render_table_rows(all_fetched_data)
            return
        filtered_list = [row for row in all_fetched_data if search_keyword in row.get("key_code", "").upper()]
        render_table_rows(filtered_list)

    # 【功能 C】統計資訊更新
    def update_statistics(data_list):
        total_count = len(data_list)
        unused_count = sum(1 for row in data_list if row.get("used_count", 0) < row.get("max_usage", 1))
        used_count = total_count - unused_count
        
        card_total.value = f"總卡密: {total_count} 張"
        card_unused.value = f"未使用: {unused_count} 張"
        card_used.value = f"已使用: {used_count} 張"

    # 【功能 D】選定卡密時的變更狀態
    def row_selected(key_code):
        nonlocal selected_key
        selected_key = key_code
        lbl_status.value = f"📌 已鎖定目標: {selected_key}"
        lbl_status.color = ft.Colors.CYAN_ACCENT
        page.update()

    # 【功能 E】一鍵複製選定的卡密
    def copy_key(e):
        if not selected_key:
            lbl_status.value = "⚠️ 請先點擊下方列表選取一個卡密！"
            lbl_status.color = ft.Colors.ORANGE_ACCENT
            page.update()
            return
        
        try:
            import tkinter as tk
            root = tk.Tk()
            root.withdraw()
            root.clipboard_clear()
            root.clipboard_append(selected_key)
            root.update()  
            root.destroy()
            
            lbl_status.value = f"📋 複製成功: {selected_key}"
            lbl_status.color = ft.Colors.LIGHT_GREEN_ACCENT
        except Exception as ex:
            lbl_status.value = "❌ 系統剪貼簿遭作業系統封鎖"
            lbl_status.color = ft.Colors.RED_ACCENT
                
        page.update()

    # 【功能 F】一鍵生成卡密
    def generate_key(e):
        try:
            try:
                input_max_usage = int(tf_max_usage.value)
                if input_max_usage <= 0: raise ValueError
            except ValueError:
                input_max_usage = 1  
                tf_max_usage.value = "1"

            try:
                input_valid_days = int(tf_valid_days.value)
                if input_valid_days <= 0: raise ValueError
            except ValueError:
                input_valid_days = 30  
                tf_valid_days.value = "30"

            lbl_status.value = "⏳ 正在雲端部署新金鑰..."
            lbl_status.color = ft.Colors.AMBER
            page.update()
            
            random_str = ''.join(random.choices(string.ascii_uppercase + string.digits, k=16))
            new_key = f"XS_{random_str}"
            
            supabase.table("keys").insert({
                "key_code": new_key,
                "max_usage": input_max_usage,
                "used_count": 0,
                "valid_days": input_valid_days,
                "hwid": None,
                "expires_at": None
            }).execute()
            
            def close_success_dialog(el):
                success_dialog.open = False
                page.update()
                load_data()
            
            success_dialog = ft.AlertDialog(
                modal=True,
                title=ft.Text("🎉 XS 系統提示", weight=ft.FontWeight.BOLD, color=ft.Colors.GREEN),
                content=ft.Text(
                    f"全新授權卡密已成功生成！\n\n"
                    f"卡密序號：\n{new_key}\n\n"
                    f"設定規格：最大次數 {input_max_usage} 次 / 授權天數 {input_valid_days} 天"
                ),
                actions=[
                    ft.TextButton("確定", on_click=close_success_dialog)
                ],
                actions_alignment=ft.MainAxisAlignment.END,
            )
            
            page.overlay.append(success_dialog)
            success_dialog.open = True
            
        except Exception as ex:
            lbl_status.value = "● 生成失敗"
            lbl_status.color = ft.Colors.RED_ACCENT
            print(traceback.format_exc())
            
        page.update()

    # 【功能 G】銷毀選定卡密
    def delete_key(e):
        nonlocal selected_key
        if not selected_key:
            warn_dialog = ft.AlertDialog(title=ft.Text("⚠️ 提示"), content=ft.Text("請先在下方資料庫列表點擊選取一個卡密！"))
            page.overlay.append(warn_dialog)
            warn_dialog.open = True
            page.update()
            return

        def confirm_delete(el):
            nonlocal selected_key
            confirm_dialog.open = False
            try:
                lbl_status.value = f"⏳ 正在蒸發數據: {selected_key}..."
                lbl_status.color = ft.Colors.AMBER
                page.update()
                
                supabase.table("keys").delete().eq("key_code", selected_key).execute()
                load_data()
                
                done_dialog = ft.AlertDialog(title=ft.Text("✅ 銷毀成功"), content=ft.Text("該卡密已徹底從後台物理蒸發！"))
                page.overlay.append(done_dialog)
                done_dialog.open = True
            except Exception as ex:
                lbl_status.value = "● 銷毀失敗"
                lbl_status.color = ft.Colors.RED_ACCENT
            page.update()

        def cancel_delete(el):
            confirm_dialog.open = False
            page.update()

        confirm_dialog = ft.AlertDialog(
            modal=True,
            title=ft.Text("⚠️ 銷毀確認", color=ft.Colors.RED_ACCENT),
            content=ft.Text(f"確定要將選定的卡密【{selected_key}】從雲端庫中永久抹除嗎？"),
            actions=[ft.TextButton("確定抹除", on_click=confirm_delete), ft.TextButton("取消", on_click=cancel_delete)],
            actions_alignment=ft.MainAxisAlignment.END,
        )
        page.overlay.append(confirm_dialog)
        confirm_dialog.open = True
        page.update()

    # ==========================================
    # 🎨 3. UI 介面自訂分頁切換邏輯
    # ==========================================
    def switch_to_home(e):
        btn_tab1.bgcolor = ACCENT_BLUE
        btn_tab1.color = ft.Colors.WHITE
        btn_tab2.bgcolor = "#2C2C30"
        btn_tab2.color = ft.Colors.GREY_400
        
        tab_home_view.visible = True
        tab_database_view.visible = False
        page.update()

    def switch_to_database(e):
        btn_tab1.bgcolor = "#2C2C30"
        btn_tab1.color = ft.Colors.GREY_400
        btn_tab2.bgcolor = ACCENT_BLUE
        btn_tab2.color = ft.Colors.WHITE
        
        tab_home_view.visible = False
        tab_database_view.visible = True
        page.update()

    # 基礎狀態與控制組件
    lbl_status = ft.Text("● 系統就緒", size=14, weight=ft.FontWeight.W_500, color=ft.Colors.CYAN_ACCENT)
    btn_ref = ft.IconButton(icon=ft.Icons.REFRESH, tooltip="刷新數據", on_click=load_data)

    # 宣告切換按鈕
    btn_tab1 = ft.Button("🏠 首頁控制面板", bgcolor=ACCENT_BLUE, color=ft.Colors.WHITE, on_click=switch_to_home)
    btn_tab2 = ft.Button("📊 雲端數據監控", bgcolor="#2C2C30", color=ft.Colors.GREY_400, on_click=switch_to_database)

    # 頂部導航列面板布局
    header_panel = ft.Container(
        content=ft.Row([
            ft.Row([
                ft.Text("XS AI SYSTEM", size=16, weight=ft.FontWeight.BOLD, color=ft.Colors.WHITE),
                ft.VerticalDivider(width=10),
                btn_tab1,
                btn_tab2
            ], spacing=10, vertical_alignment=ft.CrossAxisAlignment.CENTER),
            ft.Row([btn_ref, lbl_status], vertical_alignment=ft.CrossAxisAlignment.CENTER)
        ], alignment=ft.MainAxisAlignment.SPACE_BETWEEN),
        bgcolor=SURFACE_COLOR, padding=12, border_radius=8, margin=ft.Margin.only(bottom=15)
    )

    # --- 🏢 [內容佈局一] 首頁控制內頁 ---
    tf_max_usage = ft.TextField(value="1", label="最大使用次數", width=140, height=45, text_size=14, text_align=ft.TextAlign.CENTER)
    tf_valid_days = ft.TextField(value="30", label="卡密授權天數 (DAYS)", width=180, height=45, text_size=14, text_align=ft.TextAlign.CENTER)
    btn_gen = ft.Button("➕ 一鍵生成全新卡密", icon=ft.Icons.ADD, color=ft.Colors.WHITE, bgcolor="#1B5E20", height=45, on_click=generate_key)
    
    card_total = ft.Text("總卡密: 0 張", size=16, weight=ft.FontWeight.BOLD, color=ft.Colors.BLUE_200)
    card_unused = ft.Text("未使用: 0 張", size=16, weight=ft.FontWeight.BOLD, color=ft.Colors.GREEN_200)
    card_used = ft.Text("已使用: 0 張", size=16, weight=ft.FontWeight.BOLD, color=ft.Colors.RED_200)

    tab_home_view = ft.Column([
        ft.Text("⚙️ 授權規格動態配置", size=14, weight=ft.FontWeight.BOLD, color=ft.Colors.GREY_400),
        ft.Row([tf_max_usage, tf_valid_days, btn_gen], spacing=15),
        ft.Divider(height=40, color=ft.Colors.GREY_800),
        ft.Text("📊 即時營運數據統計", size=14, weight=ft.FontWeight.BOLD, color=ft.Colors.GREY_400),
        ft.Container(
            content=ft.Row([card_total, card_unused, card_used], spacing=40, alignment=ft.MainAxisAlignment.START),
            bgcolor="#151518", padding=20, border_radius=10, border=ft.Border.all(1, ft.Colors.GREY_900)
        )
    ], spacing=15, visible=True)

    # --- 📊 [內容佈局二] 數據監控內頁 (🔥 滾動排版優化版) ---
    tf_search = ft.TextField(label="🔍 輸入金鑰關鍵字搜尋...", width=280, height=42, text_size=13, on_change=filter_search)
    btn_copy = ft.Button("📋 複製選定卡密", icon=ft.Icons.COPY, color=ft.Colors.WHITE, bgcolor="#0D47A1", height=42, on_click=copy_key)
    btn_del = ft.Button("⚡ 銷毀卡密", icon=ft.Icons.DELETE_FOREVER, color=ft.Colors.WHITE, bgcolor="#B71C1C", height=42, on_click=delete_key)

    data_table = ft.DataTable(
        column_spacing=24,  # 拉大列與列之間的間距
        heading_row_height=45,
        data_row_min_height=40,
        columns=[
            ft.DataColumn(ft.Text("卡密序號 (KEY CODE)")),
            ft.DataColumn(ft.Text("狀態/次數 (STATUS)")),
            ft.DataColumn(ft.Text("授權天數 (DAYS)")),
            ft.DataColumn(ft.Text("鎖定機器碼 (HWID)")),
            ft.DataColumn(ft.Text("使用端 IP (IP)")),
            ft.DataColumn(ft.Text("最後登入時間 (LOGIN AT)")),
            ft.DataColumn(ft.Text("過期截止日 (EXPIRES AT)")),
            ft.DataColumn(ft.Text("建立時間 (CREATED AT)")),
        ], rows=[]
    )

    tab_database_view = ft.Column([
        ft.Row([tf_search, btn_copy, btn_del], spacing=15, alignment=ft.MainAxisAlignment.START),
        ft.Text("📋 XS LIVE DATABASE MONITOR", size=11, color=ft.Colors.GREY_500, weight=ft.FontWeight.BOLD),
        ft.Container(
            # 💡 核心優化：將 DataTable 包裹在橫向可滾動的 Row 中，完美解決文字擠壓重疊問題
            content=ft.ListView([
                ft.Row([data_table], scroll=ft.ScrollMode.ADAPTIVE)
            ], expand=True, spacing=10),
            expand=True, bgcolor="#121212", border=ft.Border.all(1, ft.Colors.GREY_800), border_radius=8, padding=12
        )
    ], spacing=15, expand=True, visible=False)

    # 📐 4. 主顯示核心群組區域
    main_content_area = ft.Column([
        tab_home_view,
        tab_database_view
    ], expand=True)

    # 部署至主視窗
    page.add(header_panel, main_content_area)
    load_data()

if __name__ == "__main__":
    ft.run(main)