#pragma once
#include "IGraphicsBackend.h"
#include <d3d11.h>
#include <SDL_syswm.h>
#include "imgui_impl_sdl2.h"
#include "player/render/IFrameBufferPool.h"
#include "imgui_impl_dx11.h"

class D3D11Backend : public IGraphicsBackend {
public:
    D3D11Backend() = default;

    // Triển khai các phương thức của IGraphicsBackend
    const char* GetMpvApiType() const override { return "d3d11"; }
    Uint32 GetWindowFlags() override { return 0; } // D3D11 không cần flag đặc biệt từ SDL

    bool InitContext(SDL_Window* window) override {
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        SDL_GetWindowWMInfo(window, &wmInfo);
        HWND hwnd = wmInfo.info.win.window;

        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 2;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        UINT createDeviceFlags = 0;
        // createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
        D3D_FEATURE_LEVEL featureLevel;
        const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
        HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &m_pSwapChain, &m_pd3dDevice, &featureLevel, &m_pd3dDeviceContext);
        if (res != S_OK) return false;

        CreateRenderTarget();
        return true;
    }

    bool InitImGuiBackend(SDL_Window* window) override {
        ImGui_ImplSDL2_InitForD3D(window);
        ImGui_ImplDX11_Init(m_pd3dDevice, m_pd3dDeviceContext);
        return true;
    }

    void BeginFrame(SDL_Window* window) override {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        const float clear_color_with_alpha[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
        m_pd3dDeviceContext->OMSetRenderTargets(1, &m_mainRenderTargetView, nullptr);
        m_pd3dDeviceContext->ClearRenderTargetView(m_mainRenderTargetView, clear_color_with_alpha);
    }

    void EndFrame(SDL_Window* window) override {
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    void SwapWindow(SDL_Window* window) override {
        m_pSwapChain->Present(1, 0); // Present with vsync
    }

    void Shutdown(bool isFinalShutdown) override {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        CleanupRenderTarget();
        if (m_pSwapChain) { m_pSwapChain->Release(); m_pSwapChain = nullptr; }
        if (m_pd3dDeviceContext) { m_pd3dDeviceContext->Release(); m_pd3dDeviceContext = nullptr; }
        if (m_pd3dDevice) { m_pd3dDevice->Release(); m_pd3dDevice = nullptr; }
    }

    // Các phương thức này không áp dụng cho D3D11, trả về giá trị mặc định/trống
    std::any CreateSubContext(SDL_Window* ownerWindow) override { return std::any(); }
    bool MakeCurrent(SDL_Window* window, const std::any& context) override { return true; }
    std::vector<mpv_render_param> GetPlayBackRenderParams(const ImVec2& size) override { return {}; }
    unsigned int GetGLInternalFormat() const override { return 0; }
    unsigned int GetGLFormat() const override { return 0; }
    unsigned int GetGLType() const override { return 0; }

    std::unique_ptr<IFrameBufferPool> CreateFrameBufferPool() override {
        // Trả về nullptr vì chúng ta chưa triển khai D3D11FrameBufferPool
        return nullptr;
    }

private:
    void CreateRenderTarget() {
        ID3D11Texture2D* pBackBuffer;
        m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
        m_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_mainRenderTargetView);
        pBackBuffer->Release();
    }

    void CleanupRenderTarget() {
        if (m_mainRenderTargetView) { m_mainRenderTargetView->Release(); m_mainRenderTargetView = nullptr; }
    }

    ID3D11Device*           m_pd3dDevice = nullptr;
    ID3D11DeviceContext*    m_pd3dDeviceContext = nullptr;
    IDXGISwapChain*         m_pSwapChain = nullptr;
    ID3D11RenderTargetView* m_mainRenderTargetView = nullptr;
};