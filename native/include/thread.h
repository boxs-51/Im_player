#pragma once
#include <functional>
#include <thread>
#include <string>

// Hàm chạy một task trong background thread
void CallThread_URLFetch(const std::string& Url , bool playNow = true , const std::string& title = "" ,const std::string& format_id = "") ;

void StartServiceThread() ;

