// sidebar_window.h
#pragma once
#include "mpv/mpv_settings.h"
#include "client_backend.h"


void OpenSidarBarPopup() ;
void RenderSidarBarPopup();
void ShowSidarBarPopup(bool&  closePopup_siderbar) ;
void RenderVideoList();
void RenderListVideoMPV();
void RenderVideoItem(VideoItem& v, float listWidth);
