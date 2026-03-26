#pragma once
#include <string>

bool ShowURLInputDialog(std::wstring& resultUrl);
std::wstring LoadLastURL();
void SaveLastURL(const std::wstring& url);
