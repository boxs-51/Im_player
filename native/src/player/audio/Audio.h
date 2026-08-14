#include "AudioCaptureManager.h"
#include "AudioProcessor.h"
#include "AudioOutputWorker.h"
#include "SpscRingBuffer.h"

class PlayerStateSystem;
class Audio {
private:
    // Buffer trung gian giữa Processor và OutputWorker (capacity 32 blocks ~ 1.36s)
    SpscRingBuffer<AudioBlock> m_processedAudioBuffer{32};

    // Các thành phần của Pipeline
    AudioCaptureManager m_audioCapture;
    AudioProcessor      m_audioProcessor;
    AudioOutputWorker   m_audioOutput;

public:
    void Init(mpv_handle* m_mpv, PlayerStateSystem* state);
    void OnUserSeek();
    void Shutdown();
};