#include "AudioCaptureManager.h"
#include "PlayerStateSystem.h"
#include "log.h"
#include "threads/thread_manager.h"
#include <iostream>

AudioCaptureManager::AudioCaptureManager() 
    : m_threadId(""), m_rawAudioBuffer(32)
{}

AudioCaptureManager::~AudioCaptureManager() {
    Shutdown();
}

std::string AudioCaptureManager::GetPipeName() const { 
    return m_pipeName; 
}

void AudioCaptureManager::UpdateFormatCacheFromState() {
    if (!m_stateSystem) return;

    // Đọc thông số Audio từ State System (Snapshot)
    AudioModel audio = m_stateSystem->GetAudioModel();
    if (audio.params.asamplerate > 0) {
        m_formatCache.sampleRate = static_cast<uint32_t>(audio.params.asamplerate);
    }
    if (audio.params.channel_count > 0) {
        m_formatCache.channels = static_cast<uint8_t>(audio.params.channel_count);
    }
}

void AudioCaptureManager::NotifySeekOrTrackChange() {
    m_currentGeneration.fetch_add(1, std::memory_order_relaxed);
    
    // Đánh dấu lại timeline PTS
    m_currentPts = 0.0;

    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, 
        std::cout << "[AudioCaptureManager] Seek/Track change notified. Generation: " 
                  << m_currentGeneration.load());
}

void AudioCaptureManager::Init(mpv_handle* mpv, PlayerStateSystem* stateSystem) {
    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, std::cout << "[AudioCaptureManager] Initializing...");
    
    m_mpv = mpv;
    m_stateSystem = stateSystem;
    m_threadId = "AudioCapture_" + std::to_string(reinterpret_cast<uintptr_t>(this));

    // Tạo Unique Pipe Name cho Instance này
    m_pipeName = "\\\\.\\pipe\\mpv_pcm_" + std::to_string(GetCurrentProcessId()) + "_" + std::to_string(reinterpret_cast<uintptr_t>(this));

    // Cập nhật cache định dạng ban đầu từ State
    UpdateFormatCacheFromState();

    // Đăng ký AO PCM với MPV xuất ra Named Pipe (Không ép samplerate/channels cố định)
    mpv_set_option_string(m_mpv, "ao", "pcm");
    mpv_set_option_string(m_mpv, "ao-pcm-file", m_pipeName.c_str());

    StartCapture();
}

void AudioCaptureManager::Shutdown() {
    StopCapture();
    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, std::cout << "[AudioCaptureManager] Shutdown complete.");
}

void AudioCaptureManager::StartCapture() {
    if (m_isRunning) {
        m_isCapturing = true;
        return;
    }

    m_isRunning = true;
    m_isCapturing = true;
    m_sequence = 0;
    m_currentPts = 0.0;
    
    m_captureThread = std::thread(&AudioCaptureManager::CaptureLoop, this);
    GetThreadManager().Register(m_threadId, &m_captureThread);
}

void AudioCaptureManager::StopCapture() {
    if (!m_isRunning.exchange(false)) {
        return;
    }

    m_isCapturing = false;

    // --- KHÔNG DÙNG MUTEX: Cancel I/O tức thì từ UI Thread ---
    HANDLE hPipe = m_atomicPipeHandle.exchange(INVALID_HANDLE_VALUE);
    if (hPipe != INVALID_HANDLE_VALUE) {
        CancelIoEx(hPipe, NULL);
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }

    if (m_captureThread.joinable()) {
        m_captureThread.join();
    }

    GetThreadManager().Unregister(m_threadId);
}

HANDLE AudioCaptureManager::CreateAudioPipe() {
    return CreateNamedPipeA(
        m_pipeName.c_str(),
        PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_WAIT,
        1,       // Max instances
        65536,   // Out buffer size
        65536,   // In buffer size
        0,       // Default timeout
        NULL
    );
}

void AudioCaptureManager::ClosePipeHandle(HANDLE hPipe) {
    if (hPipe != INVALID_HANDLE_VALUE) {
        CancelIoEx(hPipe, NULL);
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }
}

void AudioCaptureManager::CaptureLoop() {
    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, std::cout << "[AudioCaptureManager] Capture thread started.");

    HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    OVERLAPPED overlapped = { 0 };
    overlapped.hEvent = hEvent;

    AudioBlock block;

    while (m_isRunning) {
        // 1. Khởi tạo Pipe Handle cho luồng kết nối mới
        HANDLE currentPipe = CreateAudioPipe();
        if (currentPipe == INVALID_HANDLE_VALUE) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        m_atomicPipeHandle.store(currentPipe);

        // 2. Chờ MPV kết nối tới Pipe (Non-blocking Connect)
        ResetEvent(hEvent);
        BOOL connected = ConnectNamedPipe(currentPipe, &overlapped);
        if (!connected) {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING) {
                while (m_isRunning) {
                    DWORD waitResult = WaitForSingleObject(hEvent, 50); // Timeout ngắn 50ms
                    if (waitResult == WAIT_OBJECT_0) {
                        connected = TRUE;
                        break;
                    }
                }
            } else if (err == ERROR_PIPE_CONNECTED) {
                connected = TRUE;
            }
        }

        // 3. Vòng lặp Đọc dữ liệu PCM Stream
        if (connected && m_isRunning) {
            LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, std::cout << "[AudioCaptureManager] MPV connected to pipe.");

            // Cập nhật lại Cache định dạng khi MPV vừa kết nối
            UpdateFormatCacheFromState();

            const DWORD maxBytesToRead = static_cast<DWORD>(kMaxAudioSamples * sizeof(float));

            while (m_isRunning) {
                DWORD bytesRead = 0;
                ResetEvent(hEvent);

                // Đọc trực tiếp vào Buffer cố định của AudioBlock
                BOOL success = ReadFile(
                    currentPipe,
                    block.samples.data(),
                    maxBytesToRead,
                    &bytesRead,
                    &overlapped
                );

                if (!success && GetLastError() == ERROR_IO_PENDING) {
                    while (m_isRunning) {
                        DWORD waitRes = WaitForSingleObject(hEvent, 20); // Poll 20ms cực mượt
                        if (waitRes == WAIT_OBJECT_0) {
                            success = GetOverlappedResult(currentPipe, &overlapped, &bytesRead, FALSE);
                            break;
                        }
                    }
                }

                // Xử lý khi mất kết nối / MPV Pause / EOF
                if (!m_isRunning || !success || bytesRead == 0) {
                    LOG_NO_KEY(1, LogLevel::Warning, LogCategory::Audio, 
                        std::cout << "[AudioCaptureManager] Pipe disconnected or stream read ended. Reconnecting...");
                    break; 
                }

                // 4. Đóng gói AudioBlock & Đẩy vào RingBuffer
                if (m_isCapturing) {
                    const uint32_t sampleCount = bytesRead / sizeof(float);
                    
                    block.format.sampleRate = m_formatCache.sampleRate;
                    block.format.channels = m_formatCache.channels;
                    block.format.format = m_formatCache.format;

                    block.frames = sampleCount / block.format.channels;
                    block.sequence = m_sequence++;
                    block.generation = m_currentGeneration.load(std::memory_order_relaxed);
                    block.pts = m_currentPts;

                    // Tính toán timeline PTS chính xác dựa trên lượng mẫu thực tế đọc được
                    double blockDuration = static_cast<double>(block.frames) / block.format.sampleRate;
                    m_currentPts += blockDuration;

                    // Metrics tracking
                    m_metrics.blocksReceived++;
                    m_metrics.bytesReceived += bytesRead;
                    m_metrics.lastSequence = block.sequence;
                    m_metrics.lastPTS = block.pts;

                    // Đẩy dữ liệu sang SPSC RingBuffer (Lock-free)
                    if (!m_rawAudioBuffer.try_push(block)) {
                        m_metrics.blocksDropped++;
                        m_metrics.ringOverflows++;
                    }
                }
            }
        }

        // 5. Cleanup Pipe hiện tại để chuẩn bị cho chu kỳ Reconnect tiếp theo
        HANDLE hPipeToClose = m_atomicPipeHandle.exchange(INVALID_HANDLE_VALUE);
        ClosePipeHandle(hPipeToClose);
    }

    CloseHandle(hEvent);
    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, std::cout << "[AudioCaptureManager] Capture thread finished.");
}