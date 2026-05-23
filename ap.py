import datetime
import json
import os
import subprocess
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from queue import Queue
from typing import Optional, Dict, Any, Callable

import flet as ft
import requests
# --- 延遲導入管理器 ---
class LazyImporter:
    def __init__(self):
        self._imports = {}

    def lazy_import(self, module_name: str, alias: str = None):
        """延遲導入模塊"""
        def importer():
            if alias:
                exec(f"import {module_name} as {alias}")
                return locals()[alias]
            else:
                exec(f"import {module_name}")
                return __import__(module_name)
        self._imports[alias or module_name] = importer
        return self._get_module

    def _get_module(self, name: str):
        """獲取已導入的模塊"""
        if name in self._imports:
            if callable(self._imports[name]):
                self._imports[name] = self._imports[name]()
            return self._imports[name]
        raise ImportError(f"Module {name} not found")

# --- 全局延遲導入器 ---
lazy_importer = LazyImporter()

# 延遲導入大型模塊
ultralytics = lazy_importer.lazy_import("ultralytics")

# --- 線程管理器 ---
class ThreadManager:
    def __init__(self, max_workers: int = 4):
        self.executor = ThreadPoolExecutor(max_workers=max_workers, thread_name_prefix="XSPro")
        self.futures = set()
        self.ui_queue = Queue()
        self.running = True

        # 啟動UI更新處理線程
        self.ui_thread = threading.Thread(target=self._process_ui_updates, daemon=True)
        self.ui_thread.start()

    def submit_task(self, fn: Callable, *args, delay: float = 0, **kwargs):
        """提交任務到線程池"""
        if self.running:
            def delayed_task():
                if delay > 0:
                    time.sleep(delay)
                return fn(*args, **kwargs)

            future = self.executor.submit(delayed_task)
            self.futures.add(future)
            return future

    def submit_ui_task(self, ui_callback: Callable):
        """提交UI更新任務到隊列"""
        if self.running:
            self.ui_queue.put(ui_callback)

    def _process_ui_updates(self):
        """處理UI更新隊列"""
        while self.running:
            try:
                ui_callback = self.ui_queue.get(timeout=0.1)
                ui_callback()
                self.ui_queue.task_done()
            except:
                continue

    def cleanup_completed_futures(self):
        """清理已完成的任務"""
        self.futures = {f for f in self.futures if not f.done()}

    def shutdown(self):
        """關閉線程管理器"""
        self.running = False
        self.executor.shutdown(wait=True)
        self.ui_queue.join()

# --- 資源管理器 ---
class ResourceManager:
    def __init__(self):
        self.yolo_loaded = False
        self.yolo_loading = False
        self.yolo_future = None

    def ensure_yolo_loaded(self, callback: Optional[Callable] = None):
        """確保YOLO模型已加載，如果未加載則異步加載"""
        if self.yolo_loaded:
            if callback:
                callback()
            return

        if self.yolo_loading:
            # 如果正在加載，等待完成
            if self.yolo_future:
                def check_completion():
                    if self.yolo_future.done():
                        self.yolo_loaded = True
                        self.yolo_loading = False
                        if callback:
                            callback()
                    else:
                        # 繼續等待
                        thread_manager.submit_task(check_completion, delay=0.1)
                thread_manager.submit_task(check_completion)
            return

        # 開始加載
        self.yolo_loading = True
        self.yolo_future = thread_manager.submit_task(self._load_yolo_async, callback)

    def _load_yolo_async(self, callback: Optional[Callable] = None):
        """異步加載YOLO模型"""
        try:
            result = load_yolo_model()
            self.yolo_loaded = (result is not None)
            self.yolo_loading = False
            if callback:
                callback()
            return result
        except Exception as e:
            print(f"YOLO加載失敗: {e}")
            self.yolo_loading = False
            return None

    def get_yolo_status(self) -> str:
        """獲取YOLO狀態"""
        if self.yolo_loaded:
            return "YOLO 模型已就緒。"
        elif self.yolo_loading:
            return "YOLO 模型加載中..."
        else:
            if config.model_file.exists():
                return "YOLO 模型文件存在，點擊加載。"
            return "YOLO 模型文件不存在，點擊下載。"

# --- 啟動時間監控 ---
class StartupProfiler:
    def __init__(self):
        self.start_time = time.time()
        self.milestones = {}

    def mark_milestone(self, name: str):
        """記錄啟動里程碑"""
        self.milestones[name] = time.time() - self.start_time

    def get_startup_time(self) -> float:
        """獲取總啟動時間"""
        return time.time() - self.start_time

    def print_report(self):
        """打印啟動報告"""
        print("=== 應用啟動性能報告 ===")
        for milestone, time_taken in self.milestones.items():
            print(f"{milestone}: {time_taken:.3f}s")
        total_time = self.get_startup_time()
        print(f"總啟動時間: {total_time:.3f}s")
        print("=" * 30)

# --- 啟動優化配置 ---
class StartupOptimizer:
    def __init__(self):
        self.precompile_regex = True
        self.lazy_load_modules = True
        self.delayed_ui_init = True

    def optimize_imports(self):
        """優化導入"""
        if self.lazy_load_modules:
            # 已經在上面實現了延遲導入
            pass

    def precompile_patterns(self):
        """預編譯常用模式"""
        if self.precompile_regex:
            import re
            # 預編譯一些常用的正則表達式
            self.url_pattern = re.compile(r'https?://[^\s]+')
            self.email_pattern = re.compile(r'[\w\.-]+@[\w\.-]+\.\w+')

# --- 全局實例 ---
thread_manager = ThreadManager()
resource_manager = ResourceManager()
startup_optimizer = StartupOptimizer()
startup_profiler = StartupProfiler()

class AppConfig:

    def __init__(self):
        # 1. 直接填入，不要用 os.getenv
        self.supabase_url = "https://pdmxkspwnxorsoyowtgx.supabase.co"
        self.anon_key = "sb_publishable_ER9oM42esp_34cD-BXDOLA_VAphDItI"
        
        self.current_version = "2.0.0"
        self.cache_file = Path("config.json")
        self.model_url = "https://github.com/ultralytics/assets/releases/download/v8.4.0/yolo26n.pt"
        self.model_file = Path("yolo26n.pt")
        self.model: Optional[Any] = None
        self.notice_url = "http://127.0.0.1:8000/notice.txt"
        self.config_table = "config"

        # 2. 設定請求頭
        self.headers = {
            "apikey": self.anon_key,
            "Authorization": f"Bearer {self.anon_key}",
            "Content-Type": "application/json"
        }

        # 3. 先把這行註解掉，防止它報錯擋住啟動
        # self._validate_config()

    def _validate_config(self):
        """驗證配置是否有效"""
        if not self.supabase_url.startswith("https://") or "nfanrab" not in self.supabase_url:
            print("⚠️  Supabase URL 未正確配置。請設置 SUPABASE_URL 環境變數")
        if len(self.anon_key) < 50:
            print("⚠️  Supabase Key 未正確配置。請設置 SUPABASE_KEY 環境變數")

    def load_config(self) -> Dict[str, Any]:
        if self.cache_file.exists():
            try:
                return json.loads(self.cache_file.read_text(encoding="utf-8"))
            except Exception:
                return {}
        return {}

    def save_config(self, data: Dict[str, Any]):
        try:
            self.cache_file.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
        except Exception:
            pass

    def get_local_key(self) -> Optional[str]:
        return self.load_config().get("user_key")

    def set_local_key(self, key: str, expire_time: Optional[str] = None):
        data = self.load_config()
        data["user_key"] = key
        if expire_time:
            data["expire_time"] = expire_time
        self.save_config(data)

# --- 全局實例 ---
config = AppConfig()

# --- 工具函數 ---
def get_hwid() -> str:
    try:
        output = subprocess.check_output("wmic csproduct get uuid", shell=True, text=True, stderr=subprocess.DEVNULL)
        lines = [line.strip() for line in output.splitlines() if line.strip() and line.strip().lower() != "uuid"]
        return lines[0] if lines else "UNKNOWN_DEVICE"
    except Exception:
        return "UNKNOWN_DEVICE"

def download_model() -> bool:
    try:
        print("正在下載 YOLO 模型...")
        with requests.get(config.model_url, timeout=30, stream=True) as res:
            res.raise_for_status()
            with config.model_file.open("wb") as f:
                for chunk in res.iter_content(chunk_size=8192):
                    if chunk:
                        f.write(chunk)
        print("模型下載完成")
        return True
    except Exception as e:
        print(f"下載失敗: {e}")
        return False

def load_yolo_model() -> Optional[Any]:
    if config.model_file.exists():
        try:
            # 延遲導入 ultralytics 模塊並獲取 YOLO 類
            ultralytics_module = lazy_importer._get_module("ultralytics")
            config.model = ultralytics_module.YOLO(str(config.model_file))
            return config.model
        except Exception as e:
            print(f"YOLO 模型加載失敗: {e}")
            return None
    if download_model():
        try:
            ultralytics_module = lazy_importer._get_module("ultralytics")
            config.model = ultralytics_module.YOLO(str(config.model_file))
            return config.model
        except Exception as e:
            print(f"YOLO 模型加載失敗: {e}")
    return None

def get_yolo_status() -> str:
    return resource_manager.get_yolo_status()

def parse_expire_time(expire_str: str) -> Optional[datetime.datetime]:
    try:
        return datetime.datetime.strptime(expire_str, "%Y-%m-%d %H:%M:%S")
    except Exception:
        return None

# --- UI管理類 ---
class UIManager:
    def __init__(self, page: ft.Page):
        self.page = page
        self.notice_text = ft.Text("正在同步公告...", color="#58A6FF", size=13)
        self.status_text = ft.Text("", size=12, color="#FF4B4B")
        self.auth_status_text = ft.Text("授權狀態: 未登入", color="#58A6FF", size=14)
        self.expire_info_tile = None

        # UI元件
        self.notice_card = self._create_notice_card()
        self.auth_view = self._create_auth_view()

        # 視圖
        self.action_button_row = self._create_action_button_row()
        self.home_view = self._create_home_view()
        self.settings_view = self._create_settings_view()
        self.aimbot_view = self._create_aimbot_view()
        self.about_view = self._create_about_view()

        self._load_saved_auth_status()

        # 導航欄
        self.nav_bar = self._create_nav_bar()
        self.main_container = ft.Container(
            content=ft.Column([self.home_view, self.settings_view, self.aimbot_view, self.about_view]),
            padding=20, expand=True
        )

    def _create_notice_card(self) -> ft.Container:
        return ft.Container(
            content=ft.Row([
                ft.Icon(ft.Icons.CAMPAIGN, color="#00F2FF"),
                self.notice_text
            ], spacing=10),
            bgcolor="#161B22",
            padding=15,
            border_radius=12,
            border=ft.Border.all(1, "#1F2937"),
            margin=ft.Margin.only(bottom=10)
        )

    def _create_auth_view(self) -> ft.Container:
        name_input = ft.TextField(
            label="卡密", width=300, bgcolor="#161B22", focused_border_color="#00F2FF",
            prefix_icon="key", password=True, can_reveal_password=True, text_align="center"
        )

        saved_key = config.get_local_key()
        if saved_key:
            name_input.value = saved_key

        login_button = ft.FilledButton(
            content=ft.Text("登錄", size=16, weight="bold"),
            width=300, height=55, on_click=lambda e: self._verify_key(e, name_input, login_button),
            style=ft.ButtonStyle(bgcolor="#000000", color="#00F2FF", shape=ft.RoundedRectangleBorder(radius=12))
        )

        return ft.Container(
            content=ft.Column([
                ft.Text(" XS PRO ", size=32, weight="bold", color="#00F2FF"),
                ft.Text("歡迎回來", color="#58A6FF", size=9),
                ft.Container(height=10),
                name_input,
                self.status_text,
                login_button,
            ], horizontal_alignment="center", alignment="center", spacing=15),
            bgcolor="#0D1117", padding=40, border_radius=25, width=360,
            border=ft.Border.all(1, "#1F2937")
        )

    def _load_saved_auth_status(self):
        data = config.load_config()
        expire_time = data.get("expire_time")
        if expire_time:
            self._update_auth_status(expire_time)

    def _update_auth_status(self, expire_date: Optional[str] = None):
        if expire_date:
            status_text = f"授權狀態: 已登錄，將於 {expire_date} 到期"
            tile_title = "授權狀態: 已登錄"
            tile_subtitle = f"到期時間: {expire_date}"
        else:
            status_text = "授權狀態: 未登入"
            tile_title = "授權狀態: 未登入"
            tile_subtitle = "請先登錄"

        self.auth_status_text.value = status_text
        if self.expire_info_tile:
            self.expire_info_tile.title.value = tile_title
            self.expire_info_tile.subtitle.value = tile_subtitle

    def _create_home_view(self) -> ft.Column:
        return ft.Column([
            ft.Text("主頁", size=24, weight="bold", color="#00F2FF"),
            self.notice_card,
            self.action_button_row,
            self.auth_status_text,
        ], visible=True)

    def _create_settings_view(self) -> ft.Column:
        return ft.Column([
            ft.Text("設置", size=24, weight="bold", color="#00F2FF"),
        ], visible=False)

    def _create_action_button_row(self) -> ft.Row:
        return ft.Row([
            ft.FilledButton(
                content=ft.Text("啟動懸浮窗", weight="bold"), # 加粗文字
                width=160,
                height=45, # 增加一點高度更適合手機觸控
                # on_click=self._on_start_overlay, # 連結你的邏輯函數
                style=ft.ButtonStyle(
                    bgcolor="#0A84FF", 
                    color="white",
                    shape=ft.RoundedRectangleBorder(radius=8) # 稍微方正一點更有科技感
                )
            ),
            ft.FilledButton(
                content=ft.Text("關閉懸浮窗", weight="bold"),
                width=160,
                height=45,
                # on_click=self._on_stop_overlay, # 連結你的邏輯函數
                style=ft.ButtonStyle(
                    bgcolor="#10B981", 
                    color="white",
                    shape=ft.RoundedRectangleBorder(radius=8)
                )
            ),
        ], alignment=ft.MainAxisAlignment.SPACE_EVENLY, spacing=12) # 使用枚舉確保穩定

    def _create_aimbot_view(self) -> ft.Column:
        # 創建權限相關的UI元件
        shizuku_label = ft.Text("Shizuku", color="#00F2FF", size=13, weight="bold")
        cpu_label = ft.Text("CPU", color="#00F2FF", size=13, weight="bold")
        status_label = ft.Text("等待檢查...", color="#ffffff", size=13)
        self.yolo_label = ft.Text(resource_manager.get_yolo_status(), color="#00F2FF", size=13, weight="bold")

        # YOLO加載按鈕
        self.load_yolo_button = ft.Button(
            "加載YOLO模型",
            on_click=self._load_yolo_model,
            disabled=resource_manager.yolo_loaded,
            style=ft.ButtonStyle(bgcolor="#00F2FF", color="#000000")
        )

        self.expire_info_tile = ft.ListTile(
            leading=ft.Icon(ft.Icons.VERIFIED_USER, color="#00F2FF"),
            title=ft.Text("授權狀態: 未登入", color="white"),
            subtitle=ft.Text("請先登錄", color="#58A6FF"),
        )
        
        unbind_btn = ft.FilledButton(
            content=ft.Text("解綁當前設備"), # 文字要包在 ft.Text 裡面
            icon=ft.Icons.PHONELINK_ERASE, 
            on_click=self._on_unbind_click,
            style=ft.ButtonStyle(color="#FF4B4B")
        )
        
        power_card = ft.Container(
            content=ft.Column([
                ft.Row([
                    ft.Icon(ft.Icons.MEMORY, color="#ffffff"),
                    ft.Column([shizuku_label, cpu_label, status_label, self.yolo_label], spacing=2)
                ], spacing=10),
                self.load_yolo_button,
                ft.Container(
                    bgcolor="#282828", padding=10, border_radius=12,
                    content=ft.Column([self.expire_info_tile, unbind_btn])
                ),
                
                
            ], spacing=15),
            bgcolor="#282828", padding=15, border_radius=12,
            border=ft.Border.all(1, "#1F2937"), margin=ft.Margin.only(bottom=10)
        )

        return ft.Column([
            ft.Text("權限", size=28, weight="bold", color="#ffffff"),
            ft.Divider(height=10, color="transparent"),
            power_card,
        ], visible=False)

    def _create_about_view(self) -> ft.Column:
        return ft.Column([
            ft.Text("關於", size=24, weight="bold", color="#00F2FF"),
            ft.Text("XS PRO 是一款專為 Android 設計的輔助工具，提供多種功能以提升使用體驗。", color="#58A6FF", size=14),
            ft.Text(f"版本: {config.current_version}"),
            ft.Text("開發者: XS Team"),
            ft.Text("聯繫方式: support@xspro.com"), 
            ft.Card(
                    content=ft.Container(
                        bgcolor="#282828", padding=10, border_radius=12,
                        content=ft.Column([
                            ft.ListTile(
                                leading=ft.Icon(ft.Icons.INFO, color="#0284C7"),
                                title=ft.Text("軟件信息"),
                                subtitle=ft.Text(f"版本: {config.current_version}\n開發者: XS Team", color="#58A6FF"),
                            ),
                        ]),
                    )
                ),
            ft.Card(
                    content=ft.Container(
                        bgcolor="#282828", padding=10, border_radius=12,
                        content=ft.Column([
                            ft.ListTile(
                                leading=ft.Icon(ft.Icons.MEMORY, color="#0284C7"),
                                title=ft.Text("繪製引擎"),
                                subtitle=ft.Text("Flutter Native Renderer (v3.2.0)"),
                            ),
                        ])
                    )
                ),
        ], visible=False)
           
    def _create_nav_bar(self) -> ft.NavigationBar:
        return ft.NavigationBar(
            bgcolor="#0D1117",
            destinations=[
                ft.NavigationBarDestination(icon=ft.Icons.HOME, label="主頁"),
                ft.NavigationBarDestination(icon=ft.Icons.SETTINGS, label="設置"),
                ft.NavigationBarDestination(icon=ft.Icons.TRACK_CHANGES, label="權限"),
                ft.NavigationBarDestination(icon=ft.Icons.INFO, label="關於"),
            ],
            on_change=self._on_nav_change,
        )

    def _on_nav_change(self, e):
        index = e.control.selected_index
        self.home_view.visible = (index == 0)
        self.settings_view.visible = (index == 1)
        self.aimbot_view.visible = (index == 2)
        self.about_view.visible = (index == 3)
        self.page.update()


    def fetch_notice(self):
        """從 Supabase 或退回地址獲取公告"""
        def fetch_task():
            notice_text = "目前暫無公告"
            color = "#58A6FF"
            try:
                url = f"{config.supabase_url}/rest/v1/{config.config_table}?id=eq.1"
                res = requests.get(url, headers=config.headers, timeout=5)
                if res.status_code == 200:
                    data = res.json()
                    if data and len(data) > 0:
                        notice_text = data[0].get("announcement", notice_text) or notice_text
                        color = "white"
                        thread_manager.submit_ui_task(update_ui)
                        return
                res = requests.get(f"{config.notice_url}?t={time.time()}", timeout=3)
                res.encoding = 'utf-8'
                if res.status_code == 200 and res.text.strip():
                    notice_text = res.text.strip()
                    color = "white"
            except Exception:
                notice_text = "無法連接公告服務"
                color = "#FF4B4B"

            def update_ui():
                self.notice_text.value = notice_text
                self.notice_text.color = color
                self.page.update()

            thread_manager.submit_ui_task(update_ui)

        thread_manager.submit_task(fetch_task)

    def _verify_key(self, e, name_input: ft.TextField, login_button: ft.FilledButton):
        user_key = name_input.value.strip()
        if not user_key:
            def update_ui():
                self.status_text.value = "請輸入卡密"
                self.page.update()
            thread_manager.submit_ui_task(update_ui)
            return

        def update_ui_start():
            login_button.disabled = True
            login_button.content = ft.ProgressRing(width=20, height=20, stroke_width=2, color="#00F2FF")
            self.page.update()
        thread_manager.submit_ui_task(update_ui_start)

        def verify_task():
            status_msg = ""
            success = False
            expire_date = ""
            
            try:
                # 1. 取得當前本機的 HWID
                current_hwid = get_hwid()

                # 2. 調用 Supabase RPC 進行基本卡密與機器碼驗證
                url = f"{config.supabase_url}/rest/v1/rpc/login_check"
                payload = {"input_key": user_key, "input_hwid": current_hwid}
                
                res = requests.post(url, headers=config.headers, json=payload, timeout=7)
                res_data = res.json()
                
                if res_data.get("status") == "success":
                    expire_date = res_data.get('expiry_date', '')
                    config.set_local_key(user_key, expire_date)
                    success = True
                    
                    # ==========================================
                    # 🔥 🔥 【核心新增：回傳 IP 與最後登入時間】 🔥 🔥
                    # ==========================================
                    try:
                        # 異步撈取使用者的外網 IP
                        ip_res = requests.get("https://api.ipify.org", timeout=3)
                        user_ip = ip_res.text.strip() if ip_res.status_code == 200 else "未知 IP"
                    except Exception:
                        user_ip = "獲取 IP 失敗"

                    try:
                        # 使用 PATCH 方法，直接將 IP 與最後登入時間更新回 keys 表
                        update_url = f"{config.supabase_url}/rest/v1/keys?key_code=eq.{user_key}"
                        
                        # 複製原本的 headers 並增加 Prefer 回傳設定（安全防錯）
                        update_headers = config.headers.copy()
                        update_headers["Prefer"] = "return=representation"
                        
                        update_payload = {
                            "last_ip": user_ip,
                            "last_login": (datetime.datetime.now(datetime.timezone.utc) + datetime.timedelta(hours=8)).isoformat() # 帶有完整時區的 ISO 時間戳記
                        }
                        
                        requests.patch(update_url, headers=update_headers, json=update_payload, timeout=5)
                    except Exception as ex:
                        print(f"上傳 IP 與登入時間失敗: {ex}")
                    # ==========================================
                    
                else:
                    status_msg = res_data.get("message", "驗證失敗")
                    
            except Exception as e:
                print(f"驗證失敗: {e}")
                status_msg = "網路連線錯誤"

            def update_ui_result():
                if success:
                    self._update_auth_status(expire_date)
                    self.show_main_interface(expire_date)
                else:
                    self.status_text.value = status_msg
                    login_button.disabled = False
                    login_button.content = ft.Text("登錄", size=16, weight="bold")
                self.page.update()

            thread_manager.submit_ui_task(update_ui_result)

        thread_manager.submit_task(verify_task)
    



    def _load_yolo_model(self, e):
        """手動加載YOLO模型"""
        def update_ui():
            self.load_yolo_button.disabled = True
            self.load_yolo_button.text = "加載中..."
            self.yolo_label.value = "YOLO 模型加載中..."
            self.page.update()

        thread_manager.submit_ui_task(update_ui)

        def on_load_complete():
            def update_ui_complete():
                self.load_yolo_button.disabled = resource_manager.yolo_loaded
                self.load_yolo_button.text = "加載YOLO模型" if not resource_manager.yolo_loaded else "已加載"
                self.yolo_label.value = resource_manager.get_yolo_status()
                self.page.update()
            thread_manager.submit_ui_task(update_ui_complete)

        resource_manager.ensure_yolo_loaded(on_load_complete)



    def _on_unbind_click(self, e):
        """解綁當前設備"""
        user_key = config.get_local_key()
        if not user_key:
            return
            
        def unbind_task():
            try:
                url = f"{config.supabase_url}/rest/v1/rpc/unbind_device"
                payload = {"input_key": user_key}
                res = requests.post(url, headers=config.headers, json=payload, timeout=7)
                res_data = res.json()
                msg = res_data.get("message", "解綁成功")
                
                def show_msg():
                    self.page.snack_bar = ft.SnackBar(ft.Text(msg))
                    self.page.snack_bar.open = True
                    self.page.update()
                thread_manager.submit_ui_task(show_msg)
            except Exception as e:
                print(f"解綁失敗: {e}")
        
        thread_manager.submit_task(unbind_task)

    def check_update(self):
        def update_task():
            try:
                res = requests.get(f"{config.supabase_url}/rest/v1/rpc/check_version", headers=config.headers, timeout=3)
                if res.status_code == 200:
                    data = res.json()
                    if data.get("latest_version") != config.current_version:
                        def update_ui():
                            def go_update(e):
                                self.page.launch_url(data.get("download_url", ""))
                                self.page.window_destroy()

                            self.page.dialog = ft.AlertDialog(
                                modal=True, title=ft.Text("發現新版本"),
                                content=ft.Text(f"最新版本: {data.get('latest_version')}\n請更新以繼續使用。"),
                                actions=[ft.TextButton("立即下載", on_click=go_update)]
                            )
                            self.page.dialog.open = True
                            self.page.update()

                        thread_manager.submit_ui_task(update_ui)
            except:
                pass

        thread_manager.submit_task(update_task)
    
    def check_system(self):
        """檢查系統狀態、維護狀態和公告"""
        def system_task():
            try:
                url = f"{config.supabase_url}/rest/v1/config?id=eq.1"
                res = requests.get(url, headers=config.headers, timeout=5)
                if res.status_code == 200:
                    data = res.json()
                    if data and len(data) > 0:
                        config_data = data[0]
                        thread_manager.submit_ui_task(lambda: self._update_sys_ui(config_data))
            except:
                pass
        
        thread_manager.submit_task(system_task)
    
    def _update_sys_ui(self, data: Dict[str, Any]):
        """更新系統 UI（公告、維護狀態等）"""
        if 'announcement' in data:
            self.notice_text.value = data['announcement'] or "目前暫無公告"
            self.notice_text.color = "white" if data.get('announcement') else "#58A6FF"

        if data.get('is_maintenance', False):
            self.page.dialog = ft.AlertDialog(
                modal=True,
                title=ft.Text("維護中"),
                content=ft.Text(data.get('maintenance_message', '伺服器正在維護中，請稍後再試。')),
                open=True
            )

        if data.get('latest_version') and data.get('latest_version') != config.current_version:
            self.status_text.value = f"檢測到新版本 {data.get('latest_version')}"
        else:
            self.status_text.value = ""

        self.page.update()

    def show_main_interface(self, expire_date: str):
        """顯示主界面"""
        def update_ui():
            self.page.controls.clear()
            self.page.add(
                ft.Column([
                    self.main_container,
                    self.nav_bar
                ], expand=True)
            )

        thread_manager.submit_ui_task(update_ui)

# --- 主應用類 ---
class XSProApp:
    def __init__(self):
        self.ui_manager: Optional[UIManager] = None

    def run(self, page: ft.Page):
        startup_profiler.mark_milestone("應用初始化開始")

        # 创建 UI 管理器
        self.ui_manager = UIManager(page)

        # 應用啟動優化
        startup_optimizer.optimize_imports()
        startup_optimizer.precompile_patterns()
        startup_profiler.mark_milestone("啟動優化完成")

        # 應用配置
        page.title = "XS PRO - Android AS"
        page.window_width = 420
        page.window_height = 750
        page.bgcolor = "#0A0E14"
        page.theme_mode = ft.ThemeMode.DARK
        page.window_always_on_top = True
        page.window_resizable = False
        page.padding = 0

        startup_profiler.mark_milestone("頁面配置完成")


        # 設置頁面佈局
        page.horizontal_alignment = "center"
        page.vertical_alignment = "center"
        page.add(self.ui_manager.auth_view)
        startup_profiler.mark_milestone("UI佈局完成")

        # 檢查更新
        self.ui_manager.check_update()

        # 應用關閉時清理資源
        page.on_close = self._on_window_close

        startup_profiler.mark_milestone("應用啟動完成")
        startup_profiler.print_report()

    def _on_window_close(self, e):
        """應用關閉時的清理工作"""
        thread_manager.shutdown()

# --- 主入口 ---
def main(page: ft.Page):
    app = XSProApp()
    app.run(page)


if __name__ == "__main__":
    ft.run(main, view=ft.AppView.FLET_APP, host="0.0.0.0", port=8550)