// sidebar_window.h
#pragma once
#include "mpv/mpv_settings.h"
#include "services/services_client_backend.h"
#include <mpv/client.h>


void OpenSidarBarPopup() ;
void RenderSidarBarPopup();
void ShowSidarBarPopup(bool&  closePopup_siderbar) ;
void RenderVideoList();
void RenderListVideoMPV(mpv_handle* mpv);
void RenderVideoItem(VideoItem& v, float listWidth);
