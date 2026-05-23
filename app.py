import tkinter as tk
from tkinter import ttk
from tkinter import messagebox
from supabase import create_client, Client
import traceback

# ==========================================
# 🌐 1. Supabase 後端初始化設定
# ==========================================
SUPABASE_URL = "https://pdmxkspwnxorsoyowtgx.supabase.co"
SUPABASE_KEY = "sb_publishable_ER9oM42esp_34cD-BXDOLA_VAphDItI" 
supabase: Client = create_client(SUPABASE_URL, SUPABASE_KEY)

selected_key = None

# ==========================================
# ⚡ 2. 核心業務邏輯
# ==========================================
def load_data():
    global selected_key
    selected_key = None
    lbl_status.config(text="⏳ 同步雲端數據中...", fg="#808080")
    root.update()
    
    try:
        for item in tree.get_children():
            tree.delete(item)
            
        response = supabase.table("keys").select("*").execute()
        
        for row in response.data:
            tree.insert("", "end", values=(
                row.get("key_code", ""),
                row.get("status", "未使用"),
                row.get("created_at", "")
            ))
        lbl_status.config(text="● ⚖️ 雲端連線正常", fg="#2ECC71") # 柔和綠
    except Exception as ex:
        lbl_status.config(text="● ❌ 數據同步失敗", fg="#E74C3C") # 柔和紅
        print(traceback.format_exc())

def on_row_select(event):
    global selected_key
    selected_item = tree.selection()
    if selected_item:
        item_values = tree.item(selected_item, "values")
        if item_values:
            selected_key = item_values[0]
            lbl_status.config(text=f"📌 已鎖定: {selected_key}", fg="#3498DB") # 柔和藍

def delete_key():
    global selected_key
    if not selected_key:
        messagebox.showwarning("XS SYSTEM", "請先在下方表格中點選一個目標卡密！")
        return
    
    if not messagebox.askyesno("XS 高級授權", f"⚠️ 警告\n確定要永久銷毀卡密【{selected_key}】嗎？\n此操作不可逆！"):
        return
        
    try:
        lbl_status.config(text="⏳ 正在抹除數據...", fg="#F39C12") # 橙色
        root.update()
        
        supabase.table("keys").delete().eq("key_code", selected_key).execute()
        
        load_data()
        messagebox.showinfo("SUCCESS", "該卡密已成功從雲端蒸發！")
    except Exception as ex:
        lbl_status.config(text="● 銷毀失敗", fg="#E74C3C")
        messagebox.showerror("ERROR", f"連線終止:\n{str(ex)}")

# ==========================================
# 🎨 3. GUI 終極圓角渲染核心 (白金高雅主題)
# ==========================================
root = tk.Tk()
root.title("XS AI KEY MANAGER (PRO-VER)")
root.geometry("920x660")
root.configure(bg="#F5F5F7") # 淺灰白底色，接近 macOS 風格

# 🛠️ 畫布圓角按鈕生成器 (純手繪高性能渲染)
class ModernRoundedButton(tk.Canvas):
    def __init__(self, parent, text, bg, hov_bg, fg, width, height, radius=8, command=None, font_color_hov=None):
        super().__init__(parent, width=width, height=height, bg=parent["bg"], bd=0, highlightthickness=0, cursor="hand2")
        self.text = text
        self.bg = bg
        self.hov_bg = hov_bg
        self.fg = fg
        self.font_color_hov = font_color_hov if font_color_hov else fg
        self.w = width
        self.h = height
        self.r = radius
        self.command = command
        
        self.draw_button(self.bg, self.fg)
        self.bind("<Enter>", lambda e: self.draw_button(self.hov_bg, self.font_color_hov))
        self.bind("<Leave>", lambda e: self.draw_button(self.bg, self.fg))
        self.bind("<ButtonPress-1>", lambda e: self.on_click())

    def draw_button(self, color, text_color):
        self.delete("all")
        self.create_arc((0, 0, self.r*2, self.r*2), start=90, extent=90, fill=color, outline=color)
        self.create_arc((self.w-self.r*2, 0, self.w, self.r*2), start=0, extent=90, fill=color, outline=color)
        self.create_arc((0, self.h-self.r*2, self.r*2, self.h), start=180, extent=90, fill=color, outline=color)
        self.create_arc((self.w-self.r*2, self.h-self.r*2, self.w, self.h), start=270, extent=90, fill=color, outline=color)
        self.create_rectangle((self.r, 0, self.w-self.r, self.h), fill=color, outline=color)
        self.create_rectangle((0, self.r, self.w, self.h-self.r), fill=color, outline=color)
        self.create_text(self.w/2, self.h/2, text=self.text, fill=text_color, font=("Microsoft JhengHei", 10, "bold"))

    def on_click(self):
        if self.command: self.command()

# 頂部控制面板
top_frame = tk.Frame(root, bg="#FFFFFF", height=85) # 純白頂部
top_frame.pack(fill="x", side="top")
top_frame.pack_propagate(False)

# 🛠️ 圓角銷毀按鈕 (白底，紅字紅框，懸停變紅底白字)
btn_delete = ModernRoundedButton(
    top_frame, text="⚡ 銷毀選定卡密", 
    bg="#FFFFFF", hov_bg="#E74C3C", fg="#E74C3C", font_color_hov="#FFFFFF",
    width=140, height=36, radius=10, command=delete_key
)
# 模擬邊框 (Tkinter Canvas 畫圓角矩形邊框較麻煩，懸停效果不好處理，這裡用白色背景按鈕配紅色文字)
# 真正的圓角邊框需要用圖片或更複雜的 Canvas 繪製，這裡用懸停變色來強調
btn_delete.pack(side="left", padx=(25, 12), pady=24)

# 🛠️ 圓角刷新按鈕 (白底，灰字灰框，懸停變灰底白字)
btn_refresh = ModernRoundedButton(
    top_frame, text="🔄 刷新數據", 
    bg="#FFFFFF", hov_bg="#95A5A6", fg="#7F8C8D", font_color_hov="#FFFFFF",
    width=110, height=36, radius=10, command=load_data
)
btn_refresh.pack(side="left", pady=24)

# 右側狀態欄
lbl_status = tk.Label(
    top_frame, 
    text="● 系統就緒", 
    font=("Microsoft JhengHei", 10, "bold"), 
    bg="#FFFFFF", 
    fg="#7F8C8D", # 預設灰色
    anchor="w"
)
lbl_status.pack(side="right", padx=25, fill="x")

# 下方主區域
main_container = tk.Frame(root, bg="#F5F5F7", padx=25, pady=20)
main_container.pack(fill="both", expand=True)

lbl_title = tk.Label(
    main_container, 
    text="XS LIVE KEYBOARD CONTROL CENTER", 
    font=("Consolas", 10, "bold"), 
    bg="#F5F5F7", 
    fg="#A0A0A0" # 淺灰標題
)
lbl_title.pack(anchor="w", pady=(0, 12))

# 🛠️ 圓角卡片邊框渲染器 (白金主題)
card_canvas = tk.Canvas(main_container, bg="#F5F5F7", bd=0, highlightthickness=0)
card_canvas.pack(fill="both", expand=True)

def draw_card_background(e=None):
    card_canvas.delete("bg")
    w, h = card_canvas.winfo_width(), card_canvas.winfo_height()
    r = 12 # 卡片圓角半徑
    color = "#FFFFFF" # 純白卡片底色
    border_color = "#E0E0E0" # 淡灰外框

    card_canvas.create_arc((0, 0, r*2, r*2), start=90, extent=90, fill=color, outline=border_color, tags="bg")
    card_canvas.create_arc((w-r*2, 0, w, r*2), start=0, extent=90, fill=color, outline=border_color, tags="bg")
    card_canvas.create_arc((0, h-r*2, r*2, h), start=180, extent=90, fill=color, outline=border_color, tags="bg")
    card_canvas.create_arc((w-r*2, h-r*2, w, h), start=270, extent=90, fill=color, outline=border_color, tags="bg")
    card_canvas.create_rectangle((r, 0, w-r, h), fill=color, outline=color, tags="bg")
    card_canvas.create_rectangle((0, r, w, h-r), fill=color, outline=color, tags="bg")
    
    card_canvas.create_line((r, 0, w-r, 0), fill=border_color, tags="bg")
    card_canvas.create_line((r, h, w-r, h), fill=border_color, tags="bg")
    card_canvas.create_line((0, r, 0, h-r), fill=border_color, tags="bg")
    card_canvas.create_line((w, r, w, h-r), fill=border_color, tags="bg")

card_canvas.bind("<Configure>", draw_card_background)

# 調整 Treeview 白金高雅外觀樣式
style = ttk.Style()
style.theme_use("clam")

style.configure(
    "Treeview", 
    background="#FFFFFF", # 純白背景
    foreground="#333333", # 深灰文字
    fieldbackground="#FFFFFF",
    rowheight=36,
    bd=0,
    font=("Microsoft JhengHei", 10)
)
style.configure(
    "Treeview.Heading", 
    background="#F0F0F0", # 淺灰表頭
    foreground="#555555", # 中灰表頭文字
    font=("Microsoft JhengHei", 10, "bold"),
    rowheight=36,
    borderwidth=0
)
style.map(
    "Treeview", 
    background=[("selected", "#E1F5FE")], # 淺藍選中背景
    foreground=[("selected", "#0277BD")]  # 深藍選中文字
)

# 建立表格
columns = ("key", "status", "created_at")
tree = ttk.Treeview(card_canvas, columns=columns, show="headings", style="Treeview")

tree.heading("key", text="  卡密序號 (KEY CODE)", anchor="w")
tree.heading("status", text="狀態 (STATUS)")
tree.heading("created_at", text="建立時間 (CREATED AT)")

tree.column("key", width=380, anchor="w")
tree.column("status", width=130, anchor="center")
tree.column("created_at", width=240, anchor="center")

tree.bind("<<TreeviewSelect>>", on_row_select)

# 將表格直接嵌入到圓角畫布中
card_canvas.create_window(6, 6, window=tree, anchor="nw", width=852, height=520)

def resize_tree(e):
    w, h = card_canvas.winfo_width(), card_canvas.winfo_height()
    card_canvas.itemconfigure(1, width=w-12, height=h-12)
card_canvas.bind("<Configure>", lambda e: [draw_card_background(e), resize_tree(e)])

# 自動加載
root.after(150, load_data)
root.mainloop()