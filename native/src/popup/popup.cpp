#include <popup/popup.h>

ReusablePopup Popup_Url;
ReusablePopup videoInfoPopup;
ReusablePopup SettingPopup;
ReusablePopup SidarBarPopup;

std::vector<ReusablePopup*> allReusablePopups = {
    &Popup_Url,
    &videoInfoPopup,
    &SettingPopup,
    &SidarBarPopup
};

bool IsAnyPopupOpen() {
    for (auto* popup :  allReusablePopups) {
        if (popup->IsOpen()) return true;
    }
    return false;
}


std::vector<ReusablePopup*>& GetAllPopups() {
    return allReusablePopups;
}

void RenderAllPopups() {
    // Nếu vẫn còn dùng popup cũ (dạng ReusablePopup)
    if (Popup_Url.IsOpen()){
        RenderPopupOverlay_Url(Popup_Url);
    }
    if (videoInfoPopup.IsOpen()) {
        RenderVideoInfoPopup(videoInfoPopup); 
    }
    if (SettingPopup.IsOpen()) {
        RenderSettingPopup(SettingPopup); 
    }
    if (SidarBarPopup.IsOpen()) {
        RenderSidarBarPopup(SidarBarPopup);
    }
}
void OffPopup(){
    Popup_Url.Close();
    videoInfoPopup.Close();
    SidarBarPopup.Close();
    SettingPopup.Close();
}





