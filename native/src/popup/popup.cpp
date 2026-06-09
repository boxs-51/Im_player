#include <popup/popup.h>

ReusablePopup Popup_Url;
ReusablePopup videoInfoPopup;
ReusablePopup SettingPopup;
ReusablePopup SidarBarPopup;
ReusablePopup AudioControlPopup;
ReusablePopup TestPopup;

std::vector<ReusablePopup*> allReusablePopups = {
    &Popup_Url,
    &videoInfoPopup,
    &SettingPopup,
    &SidarBarPopup,
    &AudioControlPopup,
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
    if (AudioControlPopup.IsOpen()) {
        RenderAudioControlPopup(AudioControlPopup);
    }
    if (TestPopup.IsOpen()) {
        RenderTestPopup(TestPopup);
    }
}
void OffPopup(){
    Popup_Url.Close();
    videoInfoPopup.Close();
    SidarBarPopup.Close();
    SettingPopup.Close();
    AudioControlPopup.Close();
    TestPopup.Close();
}





