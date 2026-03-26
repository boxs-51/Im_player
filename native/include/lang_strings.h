#pragma once

#include <string>
#include <map>



// Tải file ngôn ngữ từ mã ngôn ngữ (VD: "Vietnam", "English")
void LoadStrings(const std::wstring& languageCode);

// Truy xuất chuỗi theo mã (trả về wide string)
std::wstring _(const std::wstring& key);

// Truy xuất chuỗi theo mã (trả về UTF-8 string, dùng cho ImGui)
const char* __(const std::wstring& key);

