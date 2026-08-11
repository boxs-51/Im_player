#include <popup/popup.h>

ReusablePopup Popup_Url;
ReusablePopup videoInfoPopup;
ReusablePopup SettingPopup;
ReusablePopup SidarBarPopup;
ReusablePopup TestPopup;

std::vector<ReusablePopup*> allReusablePopups = {
    &Popup_Url,
    &videoInfoPopup,
    &SettingPopup,
    &SidarBarPopup,
    &TestPopup
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

void RenderAllPopups(WindowRuntime* window) {
    // Nếu vẫn còn dùng popup cũ (dạng ReusablePopup)
    if (Popup_Url.IsOpen()){
        RenderPopupOverlay_Url(Popup_Url, window);
    }
    if (videoInfoPopup.IsOpen()) {
        RenderVideoInfoPopup(videoInfoPopup, window); 
    }
    if (SettingPopup.IsOpen()) {
        RenderSettingPopup(SettingPopup, window); 
    }
    if (SidarBarPopup.IsOpen()) {
        RenderSidarBarPopup(SidarBarPopup, window);
    }
    if (TestPopup.IsOpen()) {
        RenderTestPopup(TestPopup, window);
    }
}
void OffPopup(){
    Popup_Url.Close();
    videoInfoPopup.Close();
    SidarBarPopup.Close();
    SettingPopup.Close();
    TestPopup.Close();
}





