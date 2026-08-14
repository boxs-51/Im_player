#include "SdlAudioDevice.h"
#include "log.h"
#include <iostream>

SdlAudioDevice::SdlAudioDevice() = default;

SdlAudioDevice::~SdlAudioDevice() {
    Close();
}

bool SdlAudioDevice::Open(uint32_t sampleRate, uint8_t channels) {
    std::lock_guard<std::mutex> lock(m_lifecycleMutex);

    if (m_deviceId.load() != 0) {
        return true; // Đã mở rồi
    }

    // 1. Khởi tạo Subsystem Audio của SDL2 nếu chưa có
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            LOG_NO_KEY(1, LogLevel::Error, LogCategory::Audio,
                std::cout << "[SdlAudioDevice] Failed to init SDL Audio Subsystem: " << SDL_GetError());
            return false;
        }
    }
    m_isSdlAudioInitialized.store(true);

    // 2. Cấu hình thông số Audio Spec
    SDL_AudioSpec desiredSpec{};
    desiredSpec.freq = static_cast<int>(sampleRate);
    desiredSpec.format = AUDIO_F32SYS; // Native Endian Float32
    desiredSpec.channels = channels;
    desiredSpec.samples = 512;          // ~10.6ms ở 48kHz
    desiredSpec.callback = nullptr;
    desiredSpec.userdata = nullptr;

    SDL_AudioSpec obtainedSpec{};
    SDL_AudioDeviceID devId = SDL_OpenAudioDevice(nullptr, 0, &desiredSpec, &obtainedSpec, 0);

    if (devId == 0) {
        LOG_NO_KEY(1, LogLevel::Error, LogCategory::Audio,
            std::cout << "[SdlAudioDevice] Failed to open default audio device: " << SDL_GetError());
        return false;
    }

    // Unpause thiết bị
    SDL_PauseAudioDevice(devId, 0);

    // Atomic store để luồng Worker thấy Device ID mới
    m_deviceId.store(devId, std::memory_order_release);

    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio,
        std::cout << "[SdlAudioDevice] Opened SDL Audio Device. ID: " << devId
                  << " (" << obtainedSpec.freq << "Hz, " << (int)obtainedSpec.channels << "ch)");

    return true;
}

void SdlAudioDevice::Close() {
    std::lock_guard<std::mutex> lock(m_lifecycleMutex);

    // Nhát cắt Atomic: Đặt m_deviceId = 0 trước. 
    // Mọi lệnh Write/GetQueued từ Worker thread gọi vào sau thời điểm này sẽ lập tức return safe!
    SDL_AudioDeviceID devId = m_deviceId.exchange(0, std::memory_order_acq_rel);
    
    if (devId != 0) {
        SDL_ClearQueuedAudio(devId);
        SDL_CloseAudioDevice(devId);
        LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio,
            std::cout << "[SdlAudioDevice] Audio device closed.");
    }

    if (m_isSdlAudioInitialized.exchange(false)) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

void SdlAudioDevice::Write(const float* samples, size_t sampleCount) {
    if (!samples || sampleCount == 0) return;

    SDL_AudioDeviceID devId = m_deviceId.load(std::memory_order_acquire);
    if (devId == 0) return; // An toàn tuyệt đối, không crash

    const uint32_t bytesToWrite = static_cast<uint32_t>(sampleCount * sizeof(float));
    
    if (SDL_QueueAudio(devId, samples, bytesToWrite) < 0) {
        LOG_NO_KEY(1, LogLevel::Error, LogCategory::Audio,
            std::cout << "[SdlAudioDevice] SDL_QueueAudio failed: " << SDL_GetError());
    }
}

void SdlAudioDevice::FlushBuffers() {
    SDL_AudioDeviceID devId = m_deviceId.load(std::memory_order_acquire);
    if (devId != 0) {
        SDL_ClearQueuedAudio(devId);
    }
}

uint32_t SdlAudioDevice::GetQueuedSizeBytes() const {
    SDL_AudioDeviceID devId = m_deviceId.load(std::memory_order_acquire);
    if (devId == 0) return 0;

    return SDL_GetQueuedAudioSize(devId);
}