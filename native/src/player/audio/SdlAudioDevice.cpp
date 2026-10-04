#include "SdlAudioDevice.h"
#include "AudioTelemetry.h"
#include "log.h"
#include <iostream>

SdlAudioDevice::SdlAudioDevice() = default;

SdlAudioDevice::~SdlAudioDevice() {
    Shutdown();
}

bool SdlAudioDevice::Open(uint32_t sampleRate, uint8_t channels) {
    std::lock_guard<std::mutex> lock(m_lifecycleMutex);

    if (m_deviceId.load(std::memory_order_relaxed) != 0) {
        m_isReady.store(true, std::memory_order_release);
        return true; // Đã mở rồi
    }

    // 1. Khởi tạo Subsystem Audio của SDL2 nếu chưa có
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            LOG(1, LogLevel::Error, LogCategory::Audio,
                "[SdlAudioDevice] Failed to init SDL Audio Subsystem: %s", SDL_GetError());
            m_isReady.store(false, std::memory_order_release);
            return false;
        }
    }
    m_isSdlAudioInitialized.store(true, std::memory_order_relaxed);

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
        LOG(1, LogLevel::Error, LogCategory::Audio,
            "[SdlAudioDevice] Failed to open default audio device: %s", SDL_GetError());
        m_isReady.store(false, std::memory_order_release);
        return false;
    }

    EmitAudioTelemetryEvidence(
        "SDL_FORMAT requested_rate=%d requested_channels=%u requested_format=0x%04X obtained_rate=%d obtained_channels=%u obtained_format=0x%04X",
        desiredSpec.freq,
        static_cast<unsigned int>(desiredSpec.channels),
        static_cast<unsigned int>(AUDIO_F32SYS),
        obtainedSpec.freq,
        static_cast<unsigned int>(obtainedSpec.channels),
        static_cast<unsigned int>(obtainedSpec.format));

    if (obtainedSpec.freq != desiredSpec.freq ||
        obtainedSpec.format != AUDIO_F32SYS ||
        obtainedSpec.channels != desiredSpec.channels) {
        LOG(1, LogLevel::Error, LogCategory::Audio,
            "[SdlAudioDevice] Obtained format violates canonical contract: expected=%dHz/%uch/0x%04X obtained=%dHz/%uch/0x%04X",
            desiredSpec.freq,
            static_cast<unsigned int>(desiredSpec.channels),
            static_cast<unsigned int>(AUDIO_F32SYS),
            obtainedSpec.freq,
            static_cast<unsigned int>(obtainedSpec.channels),
            static_cast<unsigned int>(obtainedSpec.format));
        EmitAudioTelemetryEvidence(
            "ERROR SDL_FORMAT_REJECT expected_rate=%d expected_channels=%u obtained_rate=%d obtained_channels=%u",
            desiredSpec.freq,
            static_cast<unsigned int>(desiredSpec.channels),
            obtainedSpec.freq,
            static_cast<unsigned int>(obtainedSpec.channels));
        SDL_CloseAudioDevice(devId);
        m_isReady.store(false, std::memory_order_release);
        return false;
    }

    // Unpause thiết bị only after the obtained format is proven canonical.
    SDL_PauseAudioDevice(devId, 0);

    // Atomic store để luồng Worker thấy Device ID và trạng thái sẵn sàng
    m_deviceId.store(devId, std::memory_order_release);
    m_isReady.store(true, std::memory_order_release);

    LOG(1, LogLevel::Info, LogCategory::Audio,
        "[SdlAudioDevice] Opened SDL Audio Device. ID: %d (%dHz, %dch, format=0x%04X)",
        devId,
        obtainedSpec.freq,
        obtainedSpec.channels,
        static_cast<unsigned int>(obtainedSpec.format));

    return true;
}

void SdlAudioDevice::Close() {
    std::lock_guard<std::mutex> lock(m_lifecycleMutex);

    m_isReady.store(false, std::memory_order_release);

    // Nhát cắt Atomic: Đặt m_deviceId = 0 trước. 
    // Mọi lệnh Write/GetQueued từ Worker thread gọi vào sau thời điểm này sẽ lập tức return safe!
    SDL_AudioDeviceID devId = m_deviceId.exchange(0, std::memory_order_acq_rel);
    
    if (devId != 0) {
        SDL_ClearQueuedAudio(devId);
        SDL_CloseAudioDevice(devId);
        LOG(1, LogLevel::Info, LogCategory::Audio,
            "[SdlAudioDevice] Audio device closed.");
    }

    if (m_isSdlAudioInitialized.exchange(false, std::memory_order_relaxed)) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

void SdlAudioDevice::Shutdown() {
    Close();
}

bool SdlAudioDevice::IsReady() const {
    return m_isReady.load(std::memory_order_acquire) && (m_deviceId.load(std::memory_order_relaxed) != 0);
}

void SdlAudioDevice::SetReady(bool ready) {
    m_isReady.store(ready, std::memory_order_release);
}

void SdlAudioDevice::Write(const float* samples, size_t sampleCount) {
    if (!samples || sampleCount == 0) return;

    SDL_AudioDeviceID devId = m_deviceId.load(std::memory_order_acquire);
    if (devId == 0 || !m_isReady.load(std::memory_order_relaxed)) return; // An toàn tuyệt đối, không crash

    const uint32_t bytesToWrite = static_cast<uint32_t>(sampleCount * sizeof(float));
    
    if (SDL_QueueAudio(devId, samples, bytesToWrite) < 0) {
        LOG(1, LogLevel::Error, LogCategory::Audio,
            "[SdlAudioDevice] SDL_QueueAudio failed: %s", SDL_GetError());
        // Đánh dấu thiết bị lỗi để Worker biết và kích hoạt Auto-Recovery
        m_isReady.store(false, std::memory_order_release);
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