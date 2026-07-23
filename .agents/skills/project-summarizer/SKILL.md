---
name: project-summarizer
description: Tự động phân tích cấu trúc mã nguồn trong thư mục và xuất ra một file Markdown (ARCHITECTURE.md hoặc PROJECT_STRUCTURE.md) tóm tắt toàn bộ cây thư mục, chức năng các module và luồng hoạt động chính.
---

# Quy Trắc Tạo File Tóm Tắt Cấu Trúc Dự Án (Project Structure Summary)

Khi được yêu cầu "tóm tắt cấu trúc thư mục" hoặc "tạo file tài liệu kiến trúc", hãy thực hiện chính xác các bước sau:

## 1. Các bước thực hiện:
1. Sử dụng MCP `filesystem` để quét toàn bộ cây thư mục hiện tại.
2. Bỏ qua các thư mục rác/build không cần thiết (ví dụ: `node_modules`, `build`, `bin`, `.git`, `out`, `.vs`, `venv`).
3. Phân tích chức năng chính của từng thư mục và các file cốt lõi (`main`, `config`, `core`, `models`, `controllers`, v.v.).
4. Tạo/Ghi đè một file Markdown tên là `PROJECT_STRUCTURE.md` tại thư mục gốc.

## 2. Định dạng chuẩn cho file `PROJECT_STRUCTURE.md`:

```markdown
# 🏛️ Tóm Tắt Cấu Trúc Dự Án

> *File này được tạo tự động để tổng quan kiến trúc mã nguồn.*

---

## 📂 1. Cây Thư Mục Tổng Quan (Directory Tree)

\`\`\`text
.
├── src/
│   ├── core/          # Xử lý logic cốt lõi
│   ├── ui/            # Giao diện người dùng
│   └── main.cpp       # Entry point
├── config/            # File cấu hình
└── README.md
\`\`\`

---

## 🛠️ 2. Chi Tiết Các Module Chính

### `src/core/`
* **Nhiệm vụ:** Quản lý logic chính, xử lý dữ liệu.
* **Các file quan trọng:**
  * `engine.cpp`: Động cơ xử lý chính.
  * `utils.h`: Các hàm tiện ích dùng chung.

### `src/ui/`
* **Nhiệm vụ:** Dựng giao diện và xử lý sự kiện người dùng.

---

## 🔄 3. Luồng Hoạt Động Chính (Execution Flow)
1. `main()` khởi tạo cấu hình từ `config/`.
2. Module UI đăng ký các callback với Core Engine.
3. Vòng lặp sự kiện chính được kích hoạt.

---
*Cập nhật gần nhất: [YYYY-MM-DD]*
\`\`\`