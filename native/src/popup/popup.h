#include "reusable_popup.h"
#include "popup_setting.h"
#include "popup_url.h"
#include "popup_about_video.h"
#include "sidebar_popup.h"

extern ReusablePopup Popup_Url;   
extern ReusablePopup videoInfoPopup;
extern ReusablePopup SettingPopup;
extern ReusablePopup SidarBarPopup;

std::vector<ReusablePopup*>& GetAllPopups() ;

bool IsAnyPopupOpen();
void RenderAllPopups();
void OffPopup();