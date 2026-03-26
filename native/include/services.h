#pragma once
#include "resolution_service.h"
#include "youtube_sevice.h"
#include "key_manager.h"
// Khởi động tất cả dịch vụ nền
void StartRuntimeServices();
void WaitForServicesReady() ;
void StopService();