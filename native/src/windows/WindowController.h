// WindowController.h
#pragma once
#include <string>

class WindowRuntime;

class WindowController {
private:
    WindowRuntime* runtime;

public:
    WindowController(WindowRuntime* run) : runtime(run) {}

    void Move(int x, int y);
    void Resize(int w, int h);
    void SetTitle(const std::string& title);
    void ToggleFullscreen();
    void Maximize();
    void Minimize();
    void Restore();
    void Close();
    void SetOpacity(float opacity);
};