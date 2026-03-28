# youtube_client_tk.py
import threading
import requests
import webbrowser
import io
import time
from PIL import Image, ImageTk
import tkinter as tk
from tkinter import ttk, messagebox

API_BASE = "http://127.0.0.1:8000"  # chỉnh nếu server chạy khác

# === Helper network functions ===
def api_get(path, params=None):
    url = f"{API_BASE}{path}"
    try:
        r = requests.get(url, params=params, timeout=10)
        r.raise_for_status()
        return r.json()
    except Exception as e:
        raise RuntimeError(f"API GET error: {e}")

def api_post(path, json_body=None, params=None):
    url = f"{API_BASE}{path}"
    try:
        r = requests.post(url, json=json_body, params=params, timeout=10)
        r.raise_for_status()
        return r.json()
    except Exception as e:
        raise RuntimeError(f"API POST error: {e}")

def fetch_image_bytes(url):
    try:
        r = requests.get(url, timeout=8)
        r.raise_for_status()
        return r.content
    except:
        return None

# === GUI App ===
class YouTubeClientApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("YouTube Service Client")
        self.geometry("1000x700")
        self.minsize(900, 600)

        # state
        self.videos = []          # current list of video dicts
        self.nextPageToken = None
        self.selected_video = None
        self.thumbnail_image = None  # keep ref to PhotoImage
        self.is_fetching = False

        self.create_widgets()

    def create_widgets(self):
        # Top frame: search + trending + keywords
        top = ttk.Frame(self)
        top.pack(fill="x", padx=8, pady=8)

        ttk.Label(top, text="Search:").pack(side="left")
        self.search_var = tk.StringVar()
        self.search_entry = ttk.Entry(top, textvariable=self.search_var, width=40)
        self.search_entry.pack(side="left", padx=(6,6))
        self.search_entry.bind("<Return>", lambda e: self.on_search())


        self.search_btn = ttk.Button(top, text="Search", command=self.on_search)
        self.search_btn.pack(side="left")

        self.trending_btn = ttk.Button(top, text="Trending", command=self.on_trending)
        self.trending_btn.pack(side="left", padx=(6,0))

        self.keywords_btn = ttk.Button(top, text="Keywords", command=self.on_keywords)
        self.keywords_btn.pack(side="left", padx=(6,0))

        self.refresh_btn = ttk.Button(top, text="Refresh", command=self.on_refresh)
        self.refresh_btn.pack(side="right")

        self.next_btn = ttk.Button(top, text="Load more", command=self.on_load_more)
        self.next_btn.pack(side="left")


        # Main split: left list, right detail
        main = ttk.Frame(self)
        main.pack(fill="both", expand=True, padx=8, pady=(0,8))

        # Left: listbox with videos
        left = ttk.Frame(main, width=480)
        left.pack(side="left", fill="both", expand=True)
        left.pack_propagate(False)

        lbl = ttk.Label(left, text="Videos")
        lbl.pack(anchor="w")
        # Use Treeview for columns: title | channel
        columns = ("title", "channel", "live", "views", "duration")
        self.tree = ttk.Treeview(left, columns=columns, show="headings", selectmode="browse")

        self.tree.heading("title", text="Title")
        self.tree.heading("channel", text="Channel")
        self.tree.heading("live", text="Live")
        self.tree.heading("views", text="Views")
        self.tree.heading("duration", text="Duration")

        self.tree.column("title", width=300, anchor="w")
        self.tree.column("channel", width=120)
        self.tree.column("live", width=50, anchor="center")
        self.tree.column("views", width=90, anchor="e")
        self.tree.column("duration", width=80, anchor="center")

        vsb = ttk.Scrollbar(left, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscroll=vsb.set)
        self.tree.pack(side="left", fill="both", expand=True)
        vsb.pack(side="left", fill="y")

        self.tree.bind("<<TreeviewSelect>>", self.on_select_video)

        # Bottom of left: pagination buttons
        pag = ttk.Frame(left)
        pag.pack(fill="x", pady=(6,0))
        self.prev_btn = ttk.Button(pag, text="Prev (not implemented)", state="disabled")
        self.prev_btn.pack(side="left")
        self.next_btn = ttk.Button(pag, text="Load more", command=self.on_load_more)
        self.next_btn.pack(side="right")

        # Right: details + thumbnail + actions
        right = ttk.Frame(main)
        right.pack(side="left", fill="both", expand=True, padx=(8,0))

        # Thumbnail
        self.thumb_lbl = ttk.Label(right, text="Thumbnail", anchor="center")
        self.thumb_lbl.pack(fill="x")
        self.canvas = tk.Canvas(right, width=320, height=180, bg="#222")
        self.canvas.pack(pady=(6,6))

        # Meta info
        self.title_text = tk.Text(right, height=3, wrap="word")
        self.title_text.pack(fill="x")
        self.title_text.configure(state="disabled")

        self.channel_var = tk.StringVar(value="")
        ttk.Label(right, textvariable=self.channel_var).pack(anchor="w", pady=(4,0))
        
        self.live_var = tk.StringVar(value="")
        ttk.Label(right, textvariable=self.live_var).pack(anchor="w")

        self.views_var = tk.StringVar(value="")
        ttk.Label(right, textvariable=self.views_var).pack(anchor="w")

        self.duration_var = tk.StringVar(value="")
        ttk.Label(right, textvariable=self.duration_var).pack(anchor="w")

        # Action buttons
        act = ttk.Frame(right)
        act.pack(fill="x", pady=(8,0))
        self.open_btn = ttk.Button(act, text="Open in browser", command=self.open_in_browser, state="disabled")
        self.open_btn.pack(side="left")
        self.watch_btn = ttk.Button(act, text="Mark as watched", command=self.mark_watched, state="disabled")
        self.watch_btn.pack(side="left", padx=(6,0))

        # Keywords list
        ttk.Label(right, text="Top Keywords").pack(anchor="w", pady=(10,0))
        self.kw_listbox = tk.Listbox(right, height=6)
        self.kw_listbox.pack(fill="x", pady=(2,0))

        # Status bar
        self.status_var = tk.StringVar(value="Ready")
        status = ttk.Label(self, textvariable=self.status_var, relief="sunken", anchor="w")
        status.pack(side="bottom", fill="x")

        self.list_status_var = tk.StringVar(value="0 / 0 | Cache: 0")
        self.list_status_lbl = ttk.Label(self, textvariable=self.list_status_var, anchor="w")
        self.list_status_lbl.pack(fill="x", pady=(2,0))

    # === UI helpers ===
    def update_list_status(self):
        total = len(self.videos)
        sel = self.tree.selection()
        idx = int(sel[0]) + 1 if sel else 0
        # Lấy số video cache từ server
        try:
            res = api_get("/all-keys")  # hoặc endpoint khác nếu cache count
            cache_count = len(res.get("keys", []))
        except:
            cache_count = 0
        self.list_status_var.set(f"{idx} / {total} | Cache: {cache_count}")

    def set_status(self, text):
        self.status_var.set(text)

    def set_fetching(self, val: bool):
        self.is_fetching = val
        if val:
            self.search_btn.config(state="disabled")
            self.trending_btn.config(state="disabled")
            self.keywords_btn.config(state="disabled")
            self.refresh_btn.config(state="disabled")
            self.next_btn.config(state="disabled")
            self.set_status("Fetching...")
        else:
            self.search_btn.config(state="normal")
            self.trending_btn.config(state="normal")
            self.keywords_btn.config(state="normal")
            self.refresh_btn.config(state="normal")
            self.next_btn.config(state="normal")
            self.set_status("Ready")

    def clear_videos(self):
        for i in self.tree.get_children():
            self.tree.delete(i)
        self.videos = []
        self.nextPageToken = None
        self.update_list_status()

    def populate_videos(self, videos):
        # videos: list of dict
        start_index = len(self.videos)
        for idx, v in enumerate(videos):
            self.videos.append(v)
            self.tree.insert(
                "",
                "end",
                iid=str(start_index + idx),
                values=(
                    v.get("title", ""),
                    v.get("channel", ""),
                    "LIVE" if v.get("is_live") else "",
                    v.get("views", ""),
                    v.get("duration", "")
                )
            )
        self.update_list_status()

    # === Actions ===
    def on_search(self):
        q = self.search_var.get().strip()
        if not q:
            messagebox.showinfo("Search", "Nhập từ khoá để tìm kiếm.")
            return
        self.clear_videos()
        self.set_fetching(True)
        threading.Thread(target=self._do_search, args=(q,), daemon=True).start()

    def _do_search(self, q):
        try:
            res = api_get("/search", params={"q": q})
            videos = res.get("videos", []) + res.get("shorts", [])
            token = res.get("nextPageToken")
            # update UI in main thread
            self.after(0, lambda: self._finish_search(videos, token))
        except Exception as e:
            self.after(0, lambda: messagebox.showerror("Error", str(e)))
            self.after(0, lambda: self.set_fetching(False))

    def _finish_search(self, videos, token):
        self.populate_videos(videos)
        self.nextPageToken = token
        self.set_fetching(False)

    def on_trending(self):
        self.clear_videos()
        self.set_fetching(True)
        threading.Thread(target=self._do_trending, daemon=True).start()

    def _do_trending(self):
        try:
            res = api_get("/trending")
            videos = res.get("videos", []) + res.get("shorts", [])
            token = res.get("nextPageToken")
            self.after(0, lambda: self._finish_search(videos, token))
        except Exception as e:
            self.after(0, lambda: messagebox.showerror("Error", str(e)))
            self.after(0, lambda: self.set_fetching(False))

    def on_keywords(self):
        self.set_fetching(True)
        threading.Thread(target=self._do_keywords, daemon=True).start()

    def _do_keywords(self):
        try:
            res = api_get("/keywords")
            top = res.get("top_keywords", [])
            # update
            self.after(0, lambda: self._finish_keywords(top))
        except Exception as e:
            self.after(0, lambda: messagebox.showerror("Error", str(e)))
            self.after(0, lambda: self.set_fetching(False))

    def _finish_keywords(self, top):
        self.kw_listbox.delete(0, tk.END)
        for k, cnt in top:
            self.kw_listbox.insert(tk.END, f"{k} ({cnt})")
        self.set_fetching(False)

    def on_refresh(self):
        # reload currently showing (if search has value, re-run search, else re-run trending)
        q = self.search_var.get().strip()
        if q:
            self.on_search()
        else:
            self.on_trending()

    def on_load_more(self):
        # if nextPageToken available, append more
        if not self.nextPageToken:
            messagebox.showinfo("Info", "No nextPageToken available")
            return
        self.set_fetching(True)
        # Determine whether we were using search or trending
        q = self.search_var.get().strip()
        threading.Thread(target=self._do_load_more, args=(q, self.nextPageToken), daemon=True).start()

    def _do_load_more(self, q, token):
        try:
            if q:
                res = api_get("/search", params={"q": q, "pageToken": token})
            else:
                res = api_get("/trending", params={"pageToken": token})
            videos = res.get("videos", []) + res.get("shorts", [])
            token = res.get("nextPageToken")
            self.after(0, lambda: self._finish_load_more(videos, token))
        except Exception as e:
            self.after(0, lambda: messagebox.showerror("Error", str(e)))
            self.after(0, lambda: self.set_fetching(False))

    def _finish_load_more(self, videos, token):
        self.populate_videos(videos)
        self.nextPageToken = token
        self.set_fetching(False)

    def on_select_video(self, event):
        sel = self.tree.selection()
        if not sel:
            return
        idx = int(sel[0])
        v = self.videos[idx]
        self.selected_video = v
        # Update details
        self.title_text.configure(state="normal")
        self.title_text.delete("1.0", "end")
        self.title_text.insert("1.0", v.get("title", ""))
        self.title_text.configure(state="disabled")
        self.channel_var.set("Channel: " + v.get("channel", ""))
        self.open_btn.config(state="normal")
        self.watch_btn.config(state="normal")
        self.live_var.set("LIVE: Yes" if v.get("is_live") else "LIVE: No")
        self.views_var.set(f"Views: {v.get('views','')}")
        self.duration_var.set(f"Duration: {v.get('duration','')}")
        # load thumbnail async
        thumb = v.get("thumbnail")
        if thumb:
            threading.Thread(target=self._load_thumbnail, args=(thumb,), daemon=True).start()
        else:
            self._clear_thumbnail()
        self.update_list_status()

    def _load_thumbnail(self, url):
        data = fetch_image_bytes(url)
        if not data:
            self.after(0, lambda: self._clear_thumbnail())
            return
        try:
            img = Image.open(io.BytesIO(data))
            # Resize to fit canvas 320x180 max while keeping aspect
            img.thumbnail((640, 360))
            # convert to PhotoImage
            photo = ImageTk.PhotoImage(img)
            # keep reference and draw
            def draw():
                self.thumbnail_image = photo
                self.canvas.delete("all")
                w = self.canvas.winfo_width()
                h = self.canvas.winfo_height()
                # center
                self.canvas.create_image(w//2, h//2, image=self.thumbnail_image, anchor="center")
            self.after(0, draw)
        except Exception:
            self.after(0, lambda: self._clear_thumbnail())

    def _clear_thumbnail(self):
        self.canvas.delete("all")
        self.thumbnail_image = None
        # draw placeholder rectangle
        w = self.canvas.winfo_width() or 320
        h = self.canvas.winfo_height() or 180
        self.canvas.create_rectangle(2,2,w-2,h-2, outline="#666")
        self.canvas.create_text(w//2, h//2, text="No thumbnail", fill="#aaa")

    def open_in_browser(self):
        if not self.selected_video:
            return
        link = self.selected_video.get("link")
        if link:
            webbrowser.open(link)

    def mark_watched(self):
        if not self.selected_video:
            return
        # Ask user for confirmation
        if not messagebox.askyesno("Mark watched", "Đánh dấu video đã xem?"):
            return
        # send to /watched
        url = self.selected_video.get("link", "")
        title = self.selected_video.get("title", "")
        self.set_fetching(True)
        threading.Thread(target=self._do_mark_watched, args=(url, title), daemon=True).start()

    def _do_mark_watched(self, url, title):
        try:
            # API /watched expects 'url' param and optional 'title'
            res = api_get("/watched", params={"url": url, "title": title})
            self.after(0, lambda: messagebox.showinfo("Watched", str(res)))
        except Exception as e:
            self.after(0, lambda: messagebox.showerror("Error", str(e)))
        finally:
            self.after(0, lambda: self.set_fetching(False))

# === Run ===
if __name__ == "__main__":
    app = YouTubeClientApp()
    app.mainloop()
