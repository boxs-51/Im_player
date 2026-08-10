// sidebar_window.h
#pragma once
#include "player/mpv_data.h"
#include "mpv/client.h"

#include "backends/client_backend.h"

void OpenSidarBarPopup(class ReusablePopup& popup) ;
void RenderSidarBarPopup(class ReusablePopup& popup);
void ShowSidarBarPopup(bool&  closePopup_siderbar) ;
void RenderVideoList();
void RenderListVideoMPV();
void RenderVideoItem(VideoItem& v, float listWidth);
