#include "reusable_popup.h"
#include "popup_setting.h"
#include "popup_url.h"
#include "popup_about_video.h"
#include "sidebar_popup.h"
#include "popup_test.h"

extern ReusablePopup Popup_Url;   
extern ReusablePopup videoInfoPopup;
extern ReusablePopup SettingPopup;
extern ReusablePopup SidarBarPopup;
extern ReusablePopup TestPopup;

std::vector<ReusablePopup*>& GetAllPopups() ;

bool IsAnyPopupOpen();
void RenderAllPopups();
void OffPopup();