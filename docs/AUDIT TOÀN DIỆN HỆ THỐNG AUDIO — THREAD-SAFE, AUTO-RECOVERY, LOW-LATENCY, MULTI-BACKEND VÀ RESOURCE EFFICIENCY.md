# AUDIT TOÀN DIỆN HỆ THỐNG AUDIO

## 1. Mục tiêu kiến trúc

Mục tiêu của hệ thống Audio không chỉ là:

```text
MPV
 ↓
Capture
 ↓
Processor
 ↓
Output
 ↓
Speaker
```

mà phải trở thành một **Audio Runtime độc lập**, có khả năng:

- Thread-safe tuyệt đối.
- Không deadlock.
- Không use-after-free.
- Không data race.
- Không để UI/control thread can thiệp trực tiếp vào realtime data plane.
- Tự phục hồi khi audio device bị mất/thay đổi.
- Tự xử lý seek / pause / resume / track change / format change.
- Có thể chuyển backend SDL2 → WASAPI → ASIO → backend khác.
- Có thể mở rộng nhiều audio driver mà không sửa pipeline core.
- Low-latency.
- Không allocation trong realtime path.
- Không block realtime thread bởi mutex hoặc I/O chậm.
- Có khả năng xử lý device disconnect/reconnect.
- Có generation để loại bỏ audio cũ sau seek/track change.
- Có telemetry/metrics để phát hiện underrun, overflow, latency và device failure.
- Tách hoàn toàn Control Plane khỏi Audio Data Plane.
- Giảm phụ thuộc vào `PlayerStateSystem`, SDL và MPV.
- Cho phép sau này dùng cùng Audio Runtime cho:
  - MPV
  - microphone
  - WASAPI capture
  - ASIO
  - game audio
  - TTS
  - subtitle/STT
  - audio analysis
  - streaming
  - recording.

---

# 2. Kết luận tổng quan

Kiến trúc hiện tại có nền tảng tốt:

```text
Audio
 ├── AudioCaptureManager
 │      ↓
 │   SPSC Raw Buffer
 │      ↓
 ├── AudioProcessor
 │      ↓
 │   SPSC Processed Buffer
 │      ↓
 └── AudioOutputWorker
        ↓
     IAudioOutputDevice
        ├── SDL
        └── WASAPI (future)
```

Đặc biệt các ý tưởng sau là đúng hướng:

- SPSC RingBuffer.
- Fixed-size `AudioBlock`.
- Không cấp phát trong Capture fast path.
- Generation ID.
- Worker thread riêng.
- Backend abstraction.
- Auto-recovery.
- Visualizer snapshot.
- Tách capture / processing / output.

Tuy nhiên hệ thống hiện tại **chưa thread-safe hoàn toàn**.

Có một số vấn đề kiến trúc nghiêm trọng.

Mức độ ưu tiên:

| Vấn đề | Mức độ |
|---|---:|
| UI gọi `FlushProcessedBuffer()` trong khi Processor/Output đang sử dụng SPSC | CRITICAL |
| `AudioProcessor` copy toàn bộ `AudioBlock` 4096 float | HIGH |
| Audio latency thực tế quá lớn so với comment | HIGH |
| `AudioOutputWorker` polling bằng sleep | HIGH |
| SDL `Close()` gọi `SDL_QuitSubSystem()` | HIGH |
| Device lifetime chưa được tách khỏi worker | HIGH |
| Backend switch phụ thuộc vào `Stop()` | HIGH |
| Format thực tế của MPV không được truyền cùng audio stream | HIGH |
| PTS hiện tại không phải timestamp chính xác của audio block | HIGH |
| Generation handling chưa atomic transaction | HIGH |
| `AudioProcessor` có thể giữ stale block trong khi seek xảy ra | HIGH |
| Output buffer bị flush bởi nhiều actor | CRITICAL |
| Auto-recovery có thể tạo latency/behavior không ổn định | MEDIUM/HIGH |
| `AudioBlock` quá lớn cho mỗi ring slot | MEDIUM |
| RingBuffer dùng modulo `%` trong hot path | MEDIUM |
| `try_push` / `try_pop` chưa bảo vệ lifecycle misuse | MEDIUM |
| Driver abstraction còn quá thấp | MEDIUM |
| Metrics chưa đủ để chẩn đoán realtime | MEDIUM |

Kiến trúc nên được chuyển từ:

```text
Shared mutable objects
+
external state polling
+
UI trực tiếp flush buffer
+
worker tự quản lifecycle
```

sang:

```text
                    CONTROL PLANE
                         │
                         ▼
                 AudioCommandQueue
                         │
                         ▼
                 AudioRuntimeOwner
                         │
        ┌────────────────┼────────────────┐
        │                │                │
        ▼                ▼                ▼
    Capture          Processor         Output
    Thread            Thread           Thread
        │                │                │
        ▼                ▼                ▼
    SPSC Raw         SPSC Processed   Device Driver
        │                │                │
        └────────────────┴────────────────┘

                    OBSERVATION PLANE
                         │
                         ▼
                   AudioMetrics
                   AudioSnapshot
```

---

# 3. Kiến trúc hiện tại

## 3.1 Pipeline hiện tại

```text
                 MPV
                  │
                  │ PCM f32le
                  ▼
        ┌─────────────────────┐
        │ AudioCaptureManager │
        │ Capture Thread      │
        └──────────┬──────────┘
                   │
                   │ SPSC
                   ▼
          Raw Audio RingBuffer
                   │
                   ▼
        ┌─────────────────────┐
        │   AudioProcessor    │
        │ Processor Thread    │
        └──────────┬──────────┘
                   │
                   │ SPSC
                   ▼
       Processed Audio RingBuffer
                   │
                   ▼
        ┌─────────────────────┐
        │ AudioOutputWorker   │
        │ Output Thread       │
        └──────────┬──────────┘
                   │
                   ▼
        IAudioOutputDevice
             │          │
             ▼          ▼
            SDL       WASAPI
```

Đây là architecture hợp lý.

Vấn đề không nằm ở việc có nhiều thread.

Vấn đề nằm ở **quyền sở hữu dữ liệu và lifecycle**.

---

# 4. Nguyên tắc kiến trúc mới

Audio Runtime cần tuân thủ 7 nguyên tắc.

## Rule 1 — Một queue chỉ có đúng một Producer và một Consumer

Ví dụ:

```text
RawQueue

Producer:
    AudioCaptureThread

Consumer:
    AudioProcessorThread
```

Tuyệt đối không:

```text
UI → RawQueue
Processor → RawQueue
Capture → RawQueue
```

Processed queue:

```text
Producer:
    AudioProcessorThread

Consumer:
    AudioOutputThread
```

UI không được gọi:

```cpp
acquire_read();
release_read();
```

trên queue này.

---

# 5. Lỗi nghiêm trọng nhất hiện tại

Trong:

```cpp
Audio::OnUserSeek()
```

hiện tại:

```cpp
m_audioCapture.NotifySeekOrTrackChange();
FlushProcessedBuffer();
```

và:

```cpp
FlushProcessedBuffer()
```

lại thực hiện:

```cpp
while ((slot = m_processedAudioBuffer.acquire_read()) != nullptr) {
    m_processedAudioBuffer.release_read();
}
```

Đây là vi phạm invariant SPSC.

Processed buffer có:

```text
Producer = AudioProcessor
Consumer = AudioOutputWorker
```

nhưng UI lại trở thành Consumer thứ hai.

Tức là:

```text
AudioProcessor
      │
      ▼
   HEAD ───────────────┐
                       │
                       ▼
                 ProcessedQueue
                       │
              ┌────────┴────────┐
              ▼                 ▼
      AudioOutputWorker        UI
         Consumer             Consumer
```

Đây không còn là SPSC.

Đây có thể tạo:

- lost block
- corrupted tail
- stale data
- duplicate audio
- audio gap
- race
- undefined behavior.

## Không được sửa bằng mutex

Không nên làm:

```cpp
std::mutex queueMutex;
```

rồi lock quanh ring buffer.

Điều đó làm mất lợi thế realtime của SPSC.

Giải pháp đúng là:

```text
UI
 │
 │ SeekCommand
 ▼
AudioRuntime
 │
 ▼
generation++
 │
 ▼
AudioProcessor
 │
 ▼
AudioOutput
 │
 ▼
Flush device
```

Tức là **Consumer duy nhất vẫn là Output thread**.

---

# 6. Thiết kế Seek mới

Seek phải được coi là một transaction.

Không nên:

```text
UI
 ├── increment generation
 └── clear queue
```

Nên:

```text
UI
   │
   ▼
SeekCommand(new position)
   │
   ▼
AudioRuntime
   │
   ├── generation++
   │
   ├── publish command
   │
   └── Capture/Processor/Output xử lý
```

Ví dụ:

```cpp
struct AudioCommand {
    enum class Type {
        Seek,
        TrackChanged,
        Pause,
        Resume,
        Stop,
        DeviceChanged,
        FormatChanged,
        Flush
    };

    Type type;
    uint64_t generation;
    double position;
};
```

Sau seek:

```text
generation 100
    ↓
Seek
    ↓
generation 101
```

Block cũ:

```text
generation = 100
```

Block mới:

```text
generation = 101
```

Output chỉ phát:

```cpp
block.generation == currentGeneration
```

---

# 7. Generation hiện tại cần cải thiện

Hiện tại:

```cpp
m_generationRequest.fetch_add(...)
```

là ý tưởng tốt.

Nhưng generation nên trở thành **runtime-wide state**, không nên chỉ nằm trong CaptureManager.

Nên có:

```cpp
class AudioEpoch {
public:
    uint64_t current() const noexcept;
    uint64_t advance() noexcept;
};
```

và tất cả pipeline dùng cùng một epoch.

```text
AudioRuntime
     │
     ▼
 AudioEpoch
     │
 ├── Capture
 ├── Processor
 └── Output
```

Điều này giúp:

```text
Seek
Track Change
Device Change
Format Change
Restart
```

đều có thể tạo epoch mới.

---

# 8. AudioBlock hiện tại

Hiện tại:

```cpp
struct AudioBlock {
    ...
    std::array<float, 4096> samples;
};
```

4096 float:

```text
4096 × 4 = 16 KB
```

Mỗi block khoảng:

```text
16 KB
```

Processed buffer:

```text
32 blocks
```

≈:

```text
512 KB
```

Raw buffer:

```text
8 blocks
```

≈:

```text
128 KB
```

Tổng chưa tính object khác:

```text
~640 KB
```

Không quá lớn.

Nhưng vấn đề lớn hơn là:

```cpp
*outSlot = *inSlot;
```

đang copy toàn bộ:

```text
4096 float
```

mỗi block.

Ở 48 kHz stereo:

```text
4096 samples / 2 = 2048 frames
2048 / 48000 ≈ 42.67 ms
```

Khoảng:

```text
23.4 block/sec
```

Copy:

```text
4096 × 4 × 23.4
≈ 383 KB/s
```

Không phải bandwidth lớn.

Nhưng realtime CPU/cache pressure vẫn không cần thiết.

---

# 9. Vấn đề latency lớn

Comment hiện tại nói:

```cpp
Capacity = 32 blocks (~1.36s)
```

Điều này chính xác.

32 × 42.67 ms:

```text
≈ 1365 ms
```

Tức là queue có thể chứa **hơn 1.3 giây audio**.

Đối với media player low latency, đây là quá lớn.

Trong khi SDL:

```cpp
samples = 512
```

≈:

```text
512 / 48000
≈ 10.67 ms
```

Hardware queue lại giới hạn khoảng:

```text
25 ms
```

Nhưng upstream queue có thể chứa:

```text
1365 ms
```

Do đó latency tổng thể không thực sự là 25 ms.

---

# 10. Buffer architecture đề xuất

Không nên dùng:

```text
Raw = 8 blocks
Processed = 32 blocks
```

cứng.

Nên sử dụng latency profile.

## Low latency

```text
Capture buffer:
2–4 blocks

Processor buffer:
3–6 blocks

Hardware:
5–15 ms
```

## Normal

```text
Capture:
4–8

Processor:
6–12

Hardware:
10–30 ms
```

## Stability

```text
Capture:
8–16

Processor:
12–24

Hardware:
20–50 ms
```

Không nên để:

```text
32 blocks
```

mặc định cho playback realtime.

---

# 11. Quan trọng: AudioBlock size

42.67 ms/block khá lớn.

Nên hướng tới:

```text
256 frames
```

hoặc:

```text
480 frames
```

hoặc:

```text
512 frames
```

Ví dụ 256 frames:

```text
256 / 48000
≈ 5.33 ms
```

512:

```text
512 / 48000
≈ 10.67 ms
```

2048:

```text
2048 / 48000
≈ 42.67 ms
```

Nếu mục tiêu:

```text
low latency + realtime analysis
```

nên ưu tiên:

```text
256–512 frames
```

---

# 12. Capture hiện tại

Capture sử dụng:

```text
MPV
 ↓
ao=pcm
 ↓
Named Pipe
 ↓
OVERLAPPED ReadFile
```

Điểm mạnh:

- Không block UI.
- Có `CancelIoEx`.
- Có worker thread.
- Không allocation mỗi block.
- Sử dụng fixed-size buffer.
- Pipe handle có atomic tracking.

Đây là nền tảng tốt.

---

# 13. Vấn đề lớn của Capture

## 13.1 Pipe chỉ là byte stream

Bạn cấu hình:

```text
f32le
```

nhưng pipe không chứa metadata format.

AudioCapture tự đọc:

```cpp
m_formatCache
```

từ:

```cpp
PlayerStateSystem
```

Điều này tạo dependency nguy hiểm:

```text
MPV actual format
        ≠
PlayerStateSystem cached format
```

Ví dụ MPV đổi:

```text
48000 Hz → 44100 Hz
```

nhưng StateSystem chưa cập nhật.

Capture sẽ decode sai:

```text
bytes
 ↓
wrong channels/sample rate
 ↓
wrong frames
 ↓
wrong PTS
```

---

# 14. Audio Format phải đi cùng Stream

Nên thiết kế:

```cpp
struct AudioStreamFormat {
    uint32_t sampleRate;
    uint16_t channels;
    AudioSampleFormat format;
};
```

và format change tạo:

```text
FormatGeneration
```

hoặc dùng chung:

```text
AudioEpoch
```

Pipeline:

```text
Capture
   │
   ├── AudioFormat
   ├── Generation
   └── PCM
   ▼
AudioBlock
```

Mỗi block phải tự mô tả:

```text
generation
format
frames
pts
samples
```

Ý tưởng `AudioBlock` hiện tại đã đi đúng hướng.

---

# 15. PTS hiện tại chưa đủ chính xác

Capture hiện tại làm:

```cpp
ReadPlayback(...)
```

sau đó:

```cpp
m_currentPts = timepos;
```

rồi tự cộng:

```cpp
frames / sampleRate
```

Điều này chỉ là estimation.

Nó có thể sai khi:

- playback speed thay đổi
- seek
- buffering
- paused
- MPV clock thay đổi
- audio output latency
- video/audio sync correction
- device latency
- dropped blocks.

Đặc biệt:

```cpp
m_currentPts = timepos;
```

mỗi block làm PTS có thể nhảy.

---

# 16. Nên tách Source Timestamp và Playback Clock

Audio block nên có:

```cpp
double sourcePts;
uint64_t generation;
```

Playback clock nên là một subsystem riêng:

```text
AudioClock
```

Ví dụ:

```cpp
struct AudioClockSnapshot {
    double mediaPosition;
    double outputPosition;
    double deviceLatency;
    double drift;
};
```

Output quyết định:

```text
play / drop / silence / resync
```

không nên Capture tự quyết định sync.

---

# 17. AudioProcessor hiện tại

Processor đang làm:

```text
Raw
 ↓
Analyze
 ↓
Copy
 ↓
Processed
```

Điểm tốt:

- Một thread.
- Không mutex.
- SPSC.
- Visualizer snapshot.
- Fixed allocation.

Nhưng có hai vấn đề.

---

# 18. Vấn đề Visualizer double-buffer

Hiện tại:

```cpp
int activeIdx = m_writeIndex.load();
outFrame = m_visualizerFrames[activeIdx];
```

và worker:

```cpp
write frame
m_writeIndex.store(targetIdx, release)
```

Cách này tốt hơn shared mutable state thông thường.

Tuy nhiên vẫn có race logic nếu:

```text
UI đọc Frame A
Processor chuyển index
Processor bắt đầu ghi Frame A
UI vẫn đang copy Frame A
```

Trong C++:

```text
UI đọc object
Processor ghi cùng object
```

có thể trở thành data race.

Double-buffer đơn giản chưa đủ để đảm bảo reader có thể giữ snapshot lâu tùy ý.

Giải pháp tốt hơn:

```text
seqlock
```

hoặc:

```text
atomic shared snapshot
```

hoặc:

```text
triple-buffer
```

Với visualizer, **triple-buffer** là lựa chọn tốt.

---

# 19. Visualizer nên tách khỏi Audio Data Plane

AudioProcessor không nên phải thực hiện quá nhiều analysis nếu output latency là mục tiêu chính.

Có thể:

```text
Raw Audio
   │
   ├──────────────► Output path
   │
   ▼
Analyzer
   │
   ▼
Visualizer Snapshot
```

hoặc:

```text
Capture
  │
  ▼
AudioBlock
  │
  ├──► Processor/Output
  │
  └──► Analysis Queue
```

Nếu FFT sau này nặng:

```text
FFT
Spectrogram
Pitch
RMS
STFT
AI transcription
```

không được phép làm nghẽn playback.

---

# 20. Vấn đề lớn: Processor copy AudioBlock

Hiện tại:

```cpp
*outSlot = *inSlot;
```

copy:

```text
~16 KB
```

Có thể tối ưu bằng:

## Option A — Move

Nếu ownership phù hợp:

```cpp
*outSlot = std::move(*inSlot);
```

Nhưng với fixed array, move vẫn có thể copy.

## Option B — Shared AudioBlock pool

Tốt hơn cho pipeline lớn:

```text
AudioBlockPool
      │
      ▼
AudioBlock*
```

Pipeline truyền pointer.

Nhưng khi dùng pointer phải quản lý lifetime rất cẩn thận.

## Option C — Single ownership transfer

Nếu capture queue chỉ chứa ownership transfer:

```text
Capture
   ↓
AudioBlock ownership
   ↓
Processor
   ↓
Output
```

có thể dùng object pool.

Đây là hướng tốt nhất nếu sau này pipeline phức tạp.

---

# 21. Output Worker hiện tại

Output Worker đang làm quá nhiều việc:

```text
device recovery
state reading
mute
volume
pause
seek
generation
PTS sync
queue management
latency control
device write
exception handling
```

Nó đang trở thành "God Thread".

Nên tách:

```text
AudioOutputWorker
 │
 ├── AudioOutputPolicy
 ├── AudioClock
 ├── DeviceManager
 ├── Resampler
 └── IAudioOutputDevice
```

---

# 22. Không nên để Output Worker đọc PlayerStateSystem liên tục

Hiện tại mỗi vòng:

```cpp
ReadPlayback()
ReadAudio()
```

Điều này khiến Audio Data Plane phụ thuộc vào:

```text
PlayerStateSystem
```

Nếu StateSystem:

- lock
- contention
- chờ thread
- cập nhật chậm
- deadlock

thì audio worker bị ảnh hưởng.

Audio thread không nên phụ thuộc mạnh vào hệ thống UI/player state.

---

# 23. Thay bằng AudioControlSnapshot

Một control thread đọc state:

```text
PlayerStateSystem
        │
        ▼
AudioControlSnapshot
        │
        ▼
Atomic publication
        │
        ▼
Audio Worker
```

Ví dụ:

```cpp
struct AudioControlSnapshot {
    uint64_t generation;

    bool paused;
    bool muted;

    float volume;
    float speed;

    double mediaPosition;
    double audioDelay;

    AudioFormat format;
};
```

Worker chỉ đọc snapshot.

Không gọi StateSystem trực tiếp.

---

# 24. Điều này giải quyết dependency

Kiến trúc mới:

```text
PlayerStateSystem
        │
        │ Control Update
        ▼
AudioControlBridge
        │
        ▼
AudioRuntime
        │
 ┌──────┼─────────┐
 ▼      ▼         ▼
Capture Processor Output
```

Audio Runtime không cần biết:

```text
PlayerStateSystem là gì.
```

Nó chỉ biết:

```text
AudioControlSnapshot
```

Đây là điểm rất quan trọng để hệ thống có thể chạy độc lập.

---

# 25. Không nên dùng mutex trong Audio realtime path

Tuyệt đối tránh:

```cpp
std::mutex
std::unique_lock
std::condition_variable
```

trong:

```text
AudioOutputThread
```

đặc biệt trong:

```cpp
Write()
```

hoặc:

```cpp
ProcessBlock()
```

Mutex chỉ nên tồn tại ở:

```text
Control Plane
Device lifecycle
Configuration
Initialization
Shutdown
```

không phải:

```text
PCM fast path
```

---

# 26. Vấn đề SDL Device hiện tại

`SdlAudioDevice` có:

```cpp
m_lifecycleMutex
```

điều này ổn cho lifecycle.

Nhưng:

```cpp
Close()
```

làm:

```cpp
SDL_QuitSubSystem(SDL_INIT_AUDIO);
```

Đây là vấn đề kiến trúc.

SDL Audio subsystem là tài nguyên global.

Nếu sau này:

```text
AudioDevice A
AudioDevice B
Microphone
Another SDL subsystem
```

cùng sử dụng SDL Audio:

```text
Device A Close()
   ↓
SDL_QuitSubSystem()
   ↓
Device B?
```

có thể phá lifecycle của device khác.

---

# 27. SDL subsystem phải có Global Audio Runtime

Nên có:

```cpp
class SdlAudioRuntime {
public:
    static bool Acquire();
    static void Release();
};
```

Reference counting:

```text
Acquire
Acquire
Acquire

count = 3

Release
count = 2

Release
count = 1

Release
count = 0
→ SDL_QuitSubSystem
```

Như vậy driver không tự quyết định global SDL lifecycle.

---

# 28. IAudioOutputDevice cần nâng cấp

Interface hiện tại:

```cpp
Open()
Close()
Write()
FlushBuffers()
GetQueuedSizeBytes()
IsReady()
SetReady()
Shutdown()
```

chưa đủ để xây hệ thống driver mạnh.

Nên hướng tới:

```cpp
class IAudioOutputDevice {
public:
    virtual ~IAudioOutputDevice() = default;

    virtual bool Open(const AudioDeviceConfig&) = 0;

    virtual void Close() noexcept = 0;

    virtual AudioDeviceStatus GetStatus() const noexcept = 0;

    virtual AudioDeviceResult Write(
        const float* samples,
        size_t frames,
        const AudioFormat& format) noexcept = 0;

    virtual void Flush() noexcept = 0;

    virtual size_t GetBufferedFrames() const noexcept = 0;

    virtual size_t GetLatencyFrames() const noexcept = 0;

    virtual bool Recover() noexcept = 0;

    virtual AudioDeviceCapabilities GetCapabilities() const = 0;
};
```

---

# 29. Không nên để Driver tự biết PlayerState

Driver chỉ nên biết:

```text
AudioFormat
PCM
device configuration
```

Không biết:

```text
PlayerStateSystem
MPV
UI
seek
subtitle
```

Đây là nguyên tắc rất quan trọng.

---

# 30. Multi-driver architecture

Nên chuyển từ:

```cpp
switch(type)
```

sang:

```text
AudioDriverRegistry
```

Ví dụ:

```text
AudioDriverRegistry
 │
 ├── SDL2Driver
 ├── WASAPIDriver
 ├── ASIODriver
 ├── CoreAudioDriver
 ├── ALSADriver
 └── NullAudioDriver
```

Factory:

```cpp
registry.Create("wasapi");
registry.Create("sdl2");
registry.Create("asio");
```

Audio core không cần biết implementation.

---

# 31. Driver Capability

Mỗi driver nên khai báo:

```cpp
struct AudioDeviceCapabilities {
    bool supportsFloat32;
    bool supportsInt16;

    bool supportsExclusive;
    bool supportsShared;

    bool supportsLowLatency;
    bool supportsDeviceHotSwap;

    uint32_t minSampleRate;
    uint32_t maxSampleRate;

    uint32_t minChannels;
    uint32_t maxChannels;
};
```

Khi đó Audio Runtime có thể tự quyết định:

```text
Input:
48000 / Float32 / Stereo

WASAPI:
48000 / Float32 / Stereo
→ direct

ASIO:
44100 only
→ resampler

SDL:
48000
→ direct
```

---

# 32. Resampler phải trở thành tầng độc lập

Nếu backend không hỗ trợ:

```text
48000 Hz
```

không được ép toàn bộ pipeline phải đổi.

Nên:

```text
Source
  ↓
Native Audio Format
  ↓
Audio Processor
  ↓
Format Converter
  ↓
Device Format
  ↓
Driver
```

Ví dụ:

```text
MPV
48k Float32 stereo

        ↓

WASAPI
44.1k Int16 stereo

        ↓

Resampler
        ↓

Format Converter
        ↓

WASAPI
```

---

# 33. Device hot-swap

Mục tiêu:

```text
Speaker A
   ↓
disconnect
   ↓
Speaker B
   ↓
Audio automatically continues
```

Pipeline:

```text
OutputWorker
      │
      ▼
DeviceManager
      │
      ├── DeviceLost
      │
      ▼
Recovery
      │
      ├── enumerate devices
      ├── choose default
      ├── negotiate format
      ├── open
      └── resume
```

Không nên restart toàn bộ:

```text
Capture
Processor
Output
```

chỉ vì device output mất.

---

# 34. Auto Recovery hiện tại

Ý tưởng hiện tại:

```cpp
if (!m_audioDevice->IsReady()) {
    recreate device
}
```

là đúng.

Nhưng:

```cpp
m_audioDevice = CreateDeviceBackend(...)
```

đang nằm trực tiếp trong Output thread.

Về lâu dài nên có:

```text
AudioDeviceManager
```

quản lý:

```text
state
recovery
retry
backoff
device enumeration
format negotiation
```

Output Worker chỉ:

```text
consume PCM
→ submit to DeviceManager
```

---

# 35. Recovery Backoff

Không nên:

```text
retry every 1 second forever
```

Nên exponential backoff:

```text
100 ms
250 ms
500 ms
1 s
2 s
5 s
10 s
```

kèm giới hạn:

```text
max retry interval = 5–10 sec
```

Khi device trở lại:

```text
reset backoff
```

---

# 36. Khi device mất, không nên giữ hàng đợi cũ vô hạn

Hiện tại Output Worker:

```cpp
if device unavailable:
    pop/drop blocks
```

Điều này hợp lý hơn việc giữ 1.3 giây audio.

Nhưng nên dùng policy rõ ràng:

```text
Device lost
   ↓
discard buffered output
   ↓
wait recovery
   ↓
new generation
   ↓
resume from current playback position
```

Không nên phát lại audio cũ sau khi device reconnect.

---

# 37. Pause hiện tại

Output:

```cpp
if (shouldSilence) {
    m_audioDevice->FlushBuffers();
}
```

được thực hiện liên tục.

Nếu player paused:

```text
Flush
sleep
Flush
sleep
Flush
```

Không cần thiết.

Nên xử lý state transition:

```text
Playing → Paused
```

chỉ flush một lần.

Sau đó:

```text
Paused
```

không cần tiếp tục flush.

---

# 38. State machine cần thiết

Audio Runtime nên có state:

```cpp
enum class AudioRuntimeState {
    Uninitialized,
    Starting,
    Running,
    Paused,
    Seeking,
    DeviceLost,
    Recovering,
    Reconfiguring,
    Stopping,
    Stopped,
    Failed
};
```

Chuyển state:

```text
Starting
   ↓
Running

Running
   ↓
Paused
   ↓
Running

Running
   ↓
Seeking
   ↓
Running

Running
   ↓
DeviceLost
   ↓
Recovering
   ↓
Running

Running
   ↓
Stopping
   ↓
Stopped
```

Không nên để từng worker tự suy luận lifecycle.

---

# 39. Audio Runtime phải là owner duy nhất

Nên có:

```cpp
class AudioRuntime {
public:
    bool Start();
    void Stop();

    void SubmitCommand(AudioCommand command);

    AudioSnapshot GetSnapshot() const;
};
```

Runtime sở hữu:

```text
Capture
Processor
Output
DeviceManager
Queues
Epoch
Metrics
ControlSnapshot
```

UI chỉ:

```cpp
runtime.SubmitCommand(...)
```

---

# 40. Control Plane vs Data Plane

Đây là thay đổi quan trọng nhất.

## Data Plane

```text
PCM
AudioBlock
RingBuffer
Output
```

Yêu cầu:

```text
fast
lock-free
non-blocking
predictable
```

## Control Plane

```text
seek
pause
resume
volume
device change
format change
backend switch
shutdown
```

Có thể:

```text
mutex
condition_variable
commands
state machine
```

Hai plane không được trộn.

---

# 41. Kiến trúc đề xuất

```text
                         UI / MPV / Player
                                │
                                ▼
                    ┌──────────────────────┐
                    │   AudioControlPlane  │
                    └──────────┬───────────┘
                               │
                        AudioCommandQueue
                               │
                               ▼
                    ┌──────────────────────┐
                    │     AudioRuntime     │
                    │   State Machine      │
                    └──────────┬───────────┘
                               │
                 ┌─────────────┼─────────────┐
                 │             │             │
                 ▼             ▼             ▼
             Capture       Processor       Output
              Thread         Thread         Thread
                 │             │             │
                 ▼             ▼             ▼
             Raw SPSC      Process SPSC    DeviceManager
                                               │
                                  ┌────────────┼─────────────┐
                                  │            │             │
                                 SDL         WASAPI         ASIO
```

---

# 42. Queue design mới

Nên có:

```text
Capture → RawQueue → Processor
Processor → OutputQueue → Output
```

và:

```text
Control → CommandQueue
```

CommandQueue không cần realtime SPSC nếu chỉ có control events.

Có thể dùng:

```text
MPSC
```

nếu:

```text
UI
MPV
System
Device Manager
```

cùng gửi command.

---

# 43. Không được Flush Queue từ bên ngoài

Thay vì:

```cpp
Audio::FlushProcessedBuffer();
```

hãy:

```cpp
AudioRuntime::SubmitCommand({
    .type = Flush
});
```

Output thread nhận:

```text
Flush
```

và chính nó:

```text
drain output queue
flush device
```

Đây là cách giữ invariant SPSC.

---

# 44. Seek transaction hoàn chỉnh

Khi user seek:

```text
UI
 │
 ▼
AudioRuntime::Seek()
 │
 ├── epoch++
 ├── controlSnapshot.position = newPosition
 └── command = SEEK
        │
        ▼
Capture Thread
 └── new generation

Processor
 └── drop old generation

Output
 ├── flush hardware
 ├── discard old blocks
 └── accept new generation
```

Không thread nào khác được trực tiếp đụng queue của thread đó.

---

# 45. Thread ownership matrix

| Resource | Owner |
|---|---|
| MPV handle | MPV/Session owner |
| Pipe handle | Capture thread |
| RawQueue producer | Capture thread |
| RawQueue consumer | Processor |
| ProcessedQueue producer | Processor |
| ProcessedQueue consumer | Output |
| Audio device | Output/DeviceManager |
| Audio device lifecycle | DeviceManager |
| Generation | AudioRuntime |
| Control snapshot | Control plane |
| Metrics | Atomic publisher |
| Visualizer | Processor/Analyzer |
| Shutdown | AudioRuntime |

Không resource nào có nhiều owner không kiểm soát.

---

# 46. Shutdown hiện tại

Shutdown hiện tại:

```text
Output Stop
Processor Stop
Capture Shutdown
Flush queue
```

thứ tự này đúng về mặt pipeline.

Nhưng phải đảm bảo:

```text
Audio::Shutdown()
```

không chạy đồng thời với:

```text
SwitchBackend()
OnUserSeek()
Start()
```

Nếu API public có thể được gọi từ nhiều thread, cần lifecycle state machine.

---

# 47. Audio Runtime Lifecycle

Nên có:

```cpp
enum class LifecycleState {
    Stopped,
    Starting,
    Running,
    Stopping
};
```

và transition atomic.

Ví dụ:

```text
Stopped
   ↓
Starting
   ↓
Running
```

Không cho:

```text
Running
 ↓
Start()
```

hoặc:

```text
Stopping
 ↓
SwitchBackend()
```

---

# 48. SwitchBackend hiện tại

Hiện tại:

```cpp
Stop();
CreateDevice();
Open();
Start();
```

Có hai vấn đề.

### Vấn đề 1

Stop worker đồng nghĩa output pipeline bị ngắt.

### Vấn đề 2

Nếu Open fail:

```text
old device đã mất
new device fail
```

không có transactional rollback.

---

# 49. Backend switch nên transactional

```text
Current Device
      │
      ▼
Create New Device
      │
      ▼
Open New Device
      │
      ├── FAIL → keep old
      │
      ▼
Pause old output
      │
      ▼
Flush old
      │
      ▼
Swap device
      │
      ▼
Close old
```

Tức là:

```text
Prepare
Commit
Cleanup
```

không phải:

```text
Destroy old
Create new
Hope it works
```

---

# 50. Double-device transition

Tốt nhất:

```text
Device A
Device B
```

có thể cùng tồn tại trong transition.

```text
Device A = current
Device B = candidate

Open B

if success:
    current = B
    close A
else:
    keep A
```

Điều này tăng reliability đáng kể.

---

# 51. AudioOutputWorker không nên catch exception chung chung quanh realtime loop

Hiện tại:

```cpp
try {
   ...
}
catch (...) {
   sleep(10ms);
}
```

Đây là safety net nhưng không nên dựa vào nó.

Trong realtime code:

```text
exception
```

thường là dấu hiệu bug hoặc allocation failure.

Nên:

```text
realtime path = noexcept
```

và exception chỉ tồn tại ở:

```text
initialization
device management
control plane
```

---

# 52. Volume processing

Hiện tại:

```cpp
volumeAdjustedBuffer.resize(sampleCount);
```

có thể allocation/reallocation.

Dù thường chỉ xảy ra khi size thay đổi, realtime path vẫn không nên có.

Nên allocate trước:

```text
maxSamples
```

khi Init.

Hoặc tốt hơn:

```text
volume nằm trong DSP Processor
```

và dùng:

```text
in-place processing
```

nếu block ownership cho phép.

---

# 53. Volume nên nằm ở đâu?

Có ba lựa chọn:

### Hardware volume

Tốt nhất nếu backend hỗ trợ.

### DSP volume

```text
Processor
```

cho consistency.

### Output Worker

Đơn giản nhưng làm output thread thêm việc.

Khuyến nghị:

```text
Audio DSP
 ├── gain
 ├── mute
 ├── limiter
 ├── resample
 └── format conversion
```

Output driver chỉ nhận:

```text
final PCM
```

---

# 54. Mute

Mute không nên:

```cpp
drop block
```

một cách tùy ý nếu vẫn cần duy trì timing.

Có thể:

```text
Mute:
    discard audio
    maintain clock
```

hoặc:

```text
Mute:
    output zeros
```

Tùy mục tiêu.

Đối với player thông thường:

```text
drop is acceptable
```

nhưng state transition phải rõ ràng.

---

# 55. AV Sync

Logic:

```cpp
if (drift > 0.3)
    drop block;
```

quá đơn giản.

Nên phân loại:

```text
drift < 10ms
    normal

10–50ms
    soft correction

50–150ms
    aggressive correction

>150ms
    resync

>300ms
    hard resync
```

Không nên chỉ:

```text
drop if >300ms
```

---

# 56. Audio Clock

Đề xuất:

```cpp
class AudioClock {
public:
    void Reset(double pts);
    void Advance(uint32_t frames, uint32_t sampleRate);

    double Position() const;
    double Drift() const;
};
```

Clock dựa trên:

```text
media clock
+
audio consumed frames
+
device queued frames
```

Ví dụ:

```text
audio playback position
=
last submitted PTS
+
played frames
-
device queued latency
```

---

# 57. Hardware latency

Hiện tại dùng:

```cpp
SDL_GetQueuedAudioSize()
```

để giới hạn queue.

Đây là thông tin hữu ích nhưng chưa phải toàn bộ latency.

Cần phân biệt:

```text
Software queue
Hardware buffer
Driver buffer
OS mixer buffer
DAC latency
```

Đối với SDL shared mode, latency thực tế có thể lớn hơn queued bytes.

---

# 58. Latency target

Nên expose:

```cpp
AudioLatencyConfig {
    targetMs;
    minBufferMs;
    maxBufferMs;
}
```

Ví dụ:

```text
Low:
target = 10ms

Normal:
target = 30ms

Stable:
target = 60ms
```

Driver tự negotiate buffer.

---

# 59. Busy polling hiện tại

Hiện tại có:

```cpp
sleep_for(1ms)
sleep_for(2ms)
sleep_for(5ms)
sleep_for(10ms)
```

Điều này:

- dễ làm tăng latency.
- CPU wakeup nhiều.
- timing không deterministic trên Windows.

Nên sử dụng:

```text
event/semaphore
```

cho control/wakeup.

Ví dụ:

```text
Audio data available
      ↓
Semaphore release
      ↓
Output thread wake
```

SPSC vẫn giữ lock-free.

---

# 60. Capture pipe cũng nên dùng event tốt hơn

Capture hiện tại dùng:

```cpp
WaitForSingleObject(..., 10)
```

loop.

Có thể cải thiện bằng:

```text
OVERLAPPED
+
event
+
CancelIoEx
```

và một shutdown event:

```text
WaitForMultipleObjects:
    [IO event]
    [Shutdown event]
```

Khi shutdown:

```text
SetEvent(shutdownEvent)
CancelIoEx(pipe)
```

Thread tỉnh ngay.

---

# 61. SPSC RingBuffer

RingBuffer hiện tại nhìn chung đúng.

Memory ordering:

```text
Producer:
write data
release head

Consumer:
acquire head
read data
release tail
```

đây là pattern chuẩn.

---

# 62. Nhưng `commit_write()` có invariant nguy hiểm

API:

```cpp
T* acquire_write();
void commit_write();
```

có thể bị misuse:

```cpp
slot = acquire_write();

... something ...

commit_write();
```

nếu giữa hai thao tác xảy ra logic lỗi.

Nên tạo RAII hoặc API rõ ownership.

Ví dụ:

```cpp
auto slot = queue.begin_write();

if (!slot)
    return;

fill(*slot);

queue.commit_write();
```

Vẫn cần invariant.

---

# 63. Không nên expose RingBuffer trực tiếp

Hiện tại:

```cpp
SpscRingBuffer<AudioBlock>& GetRawStream()
```

đang expose internal primitive.

Nên:

```text
Capture owns RawQueue
Processor receives restricted endpoint
```

Ví dụ:

```cpp
ProducerEndpoint
ConsumerEndpoint
```

để compile-time enforce direction.

---

# 64. Thiết kế Queue Endpoint

```cpp
class SpscProducer<T>;
class SpscConsumer<T>;
```

Producer chỉ có:

```cpp
acquire_write()
commit_write()
```

Consumer chỉ có:

```cpp
acquire_read()
release_read()
```

Không thể vô tình:

```text
UI acquire_read
```

nếu UI không sở hữu ConsumerEndpoint.

Đây là một cải tiến rất đáng làm.

---

# 65. Queue clear

Không nên có:

```cpp
clear()
```

public.

Nếu cần clear:

```text
Consumer thread
```

thực hiện:

```cpp
drain()
```

Ví dụ:

```cpp
while (queue.acquire_read()) {
    queue.release_read();
}
```

nhưng chỉ Consumer được gọi.

---

# 66. Overflow policy

Capture hiện tại:

```text
RawQueue full
 ↓
sleep 2ms
 ↓
retry
```

đây là backpressure.

Nhưng Audio Capture không nên chờ lâu vì MPV có thể tiếp tục gửi PCM.

Nên có policy:

```text
DROP_OLDEST
DROP_NEWEST
BLOCK
ADAPTIVE
```

Đối với realtime playback:

```text
DROP_OLDEST
```

thường hợp lý hơn khi buffer bị quá trễ.

---

# 67. Processor output full

Hiện tại:

```cpp
retry 5 times
sleep 500us
```

tối đa khoảng:

```text
2.5 ms
```

Sau đó bỏ block.

Điều này có thể tạo:

```text
audio discontinuity
```

Nhưng tốt hơn là để queue nhỏ và low latency.

Có thể dùng:

```text
drop oldest
```

khi queue vượt latency target.

---

# 68. Queue nên được đo bằng thời gian, không chỉ số block

Hiện tại:

```cpp
occupancy() > 25
```

không tốt nếu sample rate thay đổi.

Nên:

```cpp
queuedDurationMs()
```

Ví dụ:

```text
queue = 180 ms
```

thì policy:

```text
>100ms → drop oldest
>250ms → hard resync
```

---

# 69. Resource optimization

Các tài nguyên chính:

```text
PCM memory
thread stack
pipe buffers
device buffers
visualizer buffers
temporary volume buffer
```

Nên thống kê:

```text
raw queue memory
processed queue memory
device queued bytes
temporary buffer memory
peak queue occupancy
```

---

# 70. Không allocation trong fast path

Các vùng cần tuyệt đối tránh:

```text
CaptureLoop
ProcessLoop
OutputLoop
Driver::Write
```

Không:

```cpp
new
delete
vector.resize
string construction
shared_ptr allocation
mutex
condition_variable wait
```

trong realtime data path.

---

# 71. Logging

Hiện tại worker log khá nhiều.

Không nên log mỗi block.

Chỉ log:

```text
state transition
device failure
recovery
overflow burst
underflow burst
format change
generation change
```

và rate-limit.

Ví dụ:

```text
1000 overflows
```

không log 1000 dòng.

Log:

```text
Audio overflow detected:
count=1000
duration=...
```

---

# 72. Metrics cần mở rộng

Nên có:

```cpp
struct AudioMetrics {
    uint64_t capturedBlocks;
    uint64_t droppedBlocks;

    uint64_t processorUnderruns;
    uint64_t processorOverruns;

    uint64_t outputUnderruns;
    uint64_t outputOverruns;

    uint64_t deviceLostCount;
    uint64_t deviceRecoveryCount;
    uint64_t deviceRecoveryFailures;

    uint64_t seekCount;
    uint64_t formatChangeCount;

    uint64_t generationDrops;

    uint64_t staleBlocksDropped;

    uint64_t syncCorrections;
    uint64_t hardResyncs;

    uint64_t bytesProcessed;

    double avgLatencyMs;
    double peakLatencyMs;
};
```

---

# 73. Metrics phải có snapshot

UI:

```cpp
AudioMetricsSnapshot snapshot = runtime.GetMetrics();
```

không lock audio thread.

Có thể dùng atomic counters.

---

# 74. Health Monitor

Nên có:

```text
AudioHealthMonitor
```

kiểm tra:

```text
queue occupancy
underrun
overflow
device state
latency
generation
format
thread alive
```

Output:

```cpp
enum class AudioHealth {
    Healthy,
    Degraded,
    Recovering,
    Failed
};
```

---

# 75. Tự phục hồi toàn pipeline

Không chỉ device.

Nên có recovery level:

```text
Level 1:
device reopen

Level 2:
format renegotiation

Level 3:
output restart

Level 4:
processor restart

Level 5:
capture restart

Level 6:
full pipeline restart
```

Không nên full restart ngay khi có lỗi.

---

# 76. Recovery hierarchy

```text
Device failure
     │
     ▼
Reopen device
     │
     ├── success → continue
     │
     ▼
Re-negotiate format
     │
     ├── success → continue
     │
     ▼
Recreate device manager
     │
     ▼
Reset output generation
     │
     ▼
Resume
```

Chỉ khi tất cả fail:

```text
AudioRuntime = Failed
```

---

# 77. Track change

Track change nên tạo:

```text
new generation
```

và:

```text
new format
new PTS
new sequence
```

Không nên chỉ reset sequence.

Ví dụ:

```text
Track A:
generation 10

Track B:
generation 11
```

mọi block generation 10 phải bị loại.

---

# 78. Pause/resume

Pause:

```text
AudioRuntime
  ↓
Paused
```

Output:

```text
flush device
```

nhưng capture/processor có thể:

```text
continue
```

hoặc:

```text
pause upstream
```

tùy policy.

Đối với media player:

```text
pause upstream
```

thường tiết kiệm CPU hơn.

---

# 79. CPU optimization

AudioProcessor hiện tại:

```text
RMS
Peak
Spectrum fake
```

Spectrum hiện tại chỉ là:

```cpp
rms * factor
```

không phải FFT.

Nếu sau này cần FFT thật:

```text
FFT
```

không được chạy trong Output thread.

Nên:

```text
Analyzer thread
```

hoặc workload budget riêng.

---

# 80. SIMD

Sau khi architecture ổn định mới tối ưu:

```text
SSE
AVX2
AVX-512
NEON
```

cho:

```text
volume
mixing
RMS
peak
conversion
resampling
```

Không nên tối ưu SIMD trước khi giải quyết ownership/race.

---

# 81. Cache locality

`AudioBlock` lớn ~16 KB.

Ring buffer:

```text
32 × 16KB
```

~512KB.

Điều này có thể làm cache pressure.

Nếu pipeline dùng ownership transfer, có thể giảm metadata duplication.

Nhưng hiện tại fixed-size block vẫn là lựa chọn an toàn.

Ưu tiên:

```text
correctness
→ latency
→ allocation-free
→ cache optimization
→ SIMD
```

---

# 82. Driver independence

Core không được include:

```cpp
SDL.h
Windows.h
mpv/client.h
```

trừ adapter layer.

Nên:

```text
AudioCore
 ├── AudioRuntime
 ├── AudioPipeline
 ├── AudioBlock
 ├── AudioClock
 ├── AudioCommand
 ├── AudioMetrics
 └── Queue

Drivers
 ├── SDL
 ├── WASAPI
 ├── ASIO
 └── ...
```

---

# 83. Module architecture đề xuất

```text
audio/
│
├── core/
│   ├── AudioRuntime
│   ├── AudioPipeline
│   ├── AudioBlock
│   ├── AudioFormat
│   ├── AudioClock
│   ├── AudioCommand
│   ├── AudioSnapshot
│   └── AudioMetrics
│
├── queue/
│   ├── SpscRingBuffer
│   ├── SpscProducer
│   └── SpscConsumer
│
├── capture/
│   ├── IAudioSource
│   ├── MpvAudioSource
│   └── NamedPipeAudioSource
│
├── processing/
│   ├── AudioProcessor
│   ├── AudioAnalyzer
│   ├── VolumeProcessor
│   ├── Resampler
│   └── FormatConverter
│
├── output/
│   ├── AudioOutputWorker
│   ├── AudioDeviceManager
│   └── IAudioOutputDevice
│
├── drivers/
│   ├── SDL/
│   ├── WASAPI/
│   ├── ASIO/
│   └── Null/
│
└── monitoring/
    ├── AudioHealthMonitor
    └── AudioMetrics
```

---

# 84. NullAudioDevice

Rất nên có:

```text
NullAudioDevice
```

dùng cho:

```text
testing
benchmark
headless
analysis-only
subtitle extraction
```

Ví dụ:

```text
MPV
 ↓
Capture
 ↓
Processor
 ↓
NullDevice
```

Không cần speaker.

---

# 85. Đây cũng giải quyết mục tiêu Subtitle/AI

Sau này:

```text
Audio Runtime
        │
        ├── Output
        │
        ├── Visualizer
        │
        ├── Subtitle/STT
        │
        ├── Recording
        │
        └── TTS
```

Không cần lấy audio lại từ MPV.

Đây là lý do Audio Runtime nên trở thành subsystem độc lập.

---

# 86. Multi-consumer trong tương lai

SPSC chỉ phù hợp:

```text
1 producer
1 consumer
```

Nếu cần:

```text
Output
Analyzer
Recorder
STT
```

không được cho tất cả cùng đọc một SPSC queue.

Nên fan-out:

```text
Capture
   │
   ▼
Audio Distributor
   │
   ├── OutputQueue
   ├── AnalyzerQueue
   ├── RecorderQueue
   └── STTQueue
```

Hoặc reference-counted block pool.

---

# 87. Không nên dùng một queue cho mọi mục đích

Sai:

```text
AudioQueue
 ├── output
 ├── STT
 ├── visualizer
 └── recording
```

Một consumer chậm sẽ làm ảnh hưởng tất cả.

Đúng:

```text
Output = realtime critical
STT = best effort
Visualizer = best effort
Recorder = reliable
```

mỗi workload có policy riêng.

---

# 88. Priority của pipeline

Nên phân loại:

## P0 — Audio output

Không được block.

## P1 — Capture

Rất quan trọng.

## P2 — Processing

Quan trọng.

## P3 — Analyzer

Best effort.

## P4 — Visualizer/UI

Không ảnh hưởng playback.

---

# 89. CPU priority

Không nên tùy tiện dùng:

```text
REALTIME_PRIORITY_CLASS
THREAD_PRIORITY_TIME_CRITICAL
```

vì có thể làm starvation các thread khác.

Thay vào đó:

```text
normal/high priority
```

kết hợp:

```text
small buffers
event-driven wakeup
no locks
```

sẽ an toàn hơn.

---

# 90. Memory ownership model khuyến nghị

Giai đoạn đầu:

```text
fixed AudioBlock
SPSC
```

giữ nguyên.

Giai đoạn sau:

```text
AudioBlockPool
```

với:

```text
fixed number of blocks
```

Ví dụ:

```text
Pool = 64 blocks
```

Producer acquire:

```text
free block
```

Consumer release:

```text
return block
```

Không allocation runtime.

---

# 91. Audio Pool

Thiết kế:

```text
AudioBlockPool
 ├── acquire()
 └── release()
```

Có thể dùng:

```text
lock-free free-list
```

nhưng chỉ nên làm sau khi pipeline cơ bản ổn định.

Không nên tự viết lock-free MPMC phức tạp quá sớm.

---

# 92. Format negotiation

Audio Runtime phải negotiate:

```text
source format
        ↓
DSP format
        ↓
device format
```

Ví dụ:

```text
Source:
48000 / F32 / 2ch

DSP:
48000 / F32 / 2ch

Device:
44100 / S16 / 2ch
```

Pipeline:

```text
Source
 ↓
Resampler
 ↓
Float → Int16
 ↓
Driver
```

---

# 93. Sample count validation

Mỗi block phải validate:

```cpp
frames <= kMaxAudioFrames
channels <= kMaxAudioChannels
sample_count <= kMaxAudioSamples
```

Không được tin hoàn toàn vào pipe.

Ví dụ:

```cpp
if (frames > kMaxAudioFrames)
    reject;
```

Đây là boundary protection.

---

# 94. Pipe robustness

Capture phải xử lý:

```text
ERROR_BROKEN_PIPE
ERROR_NO_DATA
ERROR_OPERATION_ABORTED
ERROR_PIPE_NOT_CONNECTED
ERROR_IO_INCOMPLETE
```

và phân loại:

```text
normal disconnect
shutdown
device error
unexpected failure
```

Không nên tất cả đều log:

```text
Warning
```

---

# 95. Pipe lifecycle

Hiện tại:

```text
CreateNamedPipe
ConnectNamedPipe
ReadFile
Close
repeat
```

có thể hoạt động.

Nhưng nếu MPV reconnect nhanh:

```text
old pipe
new pipe
```

cần đảm bảo:

```text
generation++
```

để data từ connection cũ không được xem là stream mới.

---

# 96. Capture restart

Khi pipe disconnect:

```text
Capture detects disconnect
       ↓
close pipe
       ↓
increment connection generation
       ↓
recreate pipe
       ↓
wait MPV reconnect
```

Không cần restart toàn Audio Runtime.

---

# 97. Tự xử lý format change

Format change:

```text
48000 stereo
        ↓
44100 stereo
```

Capture detect:

```text
new format
```

thì:

```text
generation++
```

và gửi:

```text
FormatChanged
```

Processor reset state.

Output:

```text
flush device
renegotiate
```

---

# 98. Error classification

Không nên chỉ:

```cpp
bool success;
```

Nên:

```cpp
enum class AudioResult {
    Ok,
    WouldBlock,
    DeviceLost,
    InvalidFormat,
    Disconnected,
    Cancelled,
    RecoverableError,
    FatalError
};
```

Driver:

```cpp
AudioResult::DeviceLost
```

Audio Runtime biết cần recovery.

---

# 99. Không nên dùng `SetReady(bool)` public

Hiện tại:

```cpp
SetReady(false)
```

là một mutable flag.

Điều này cho phép code bên ngoài làm:

```text
device state = false
```

mà không thay đổi lifecycle.

Nên driver tự quản lý:

```text
state
```

và expose:

```cpp
GetStatus()
```

Output manager mới quyết định recovery.

---

# 100. Device status

```cpp
enum class AudioDeviceState {
    Closed,
    Opening,
    Ready,
    Running,
    Lost,
    Recovering,
    Failed
};
```

Đây là state thực, thay cho:

```cpp
bool m_isReady;
```

---

# 101. Thread-safe device lifetime

Không nên dựa vào:

```cpp
atomic<SDL_AudioDeviceID>
```

để giải quyết toàn bộ lifetime.

Ví dụ:

```text
Thread A:
devId = load()

Thread B:
Close(devId)

Thread A:
SDL_QueueAudio(devId)
```

Atomic ID không đảm bảo resource lifetime.

Đây là điểm rất quan trọng.

Nếu `Close()` và `Write()` có thể chạy khác thread, phải có ownership/lifecycle protocol.

Trong kiến trúc mới:

```text
ONLY Output/DeviceManager thread
```

gọi:

```text
Write
Flush
Close
Open
```

thì vấn đề này gần như được loại bỏ.

---

# 102. Quy tắc Driver Ownership

Driver API:

```text
Open
Write
Flush
Close
```

chỉ được gọi từ:

```text
Device Owner Thread
```

Không thread nào khác gọi trực tiếp.

UI muốn đổi device:

```text
Command
 ↓
DeviceManager
```

---

# 103. Đây là giải pháp thread-safe tốt hơn mutex

Không phải:

```text
mutex protect device
```

mà:

```text
single-thread ownership
```

Đây là mô hình Actor/Ownership.

Rất phù hợp với audio.

---

# 104. Audio Runtime giống Actor

Mỗi component có owner:

```text
Capture Actor
Processor Actor
Output Actor
Device Actor
```

Giao tiếp bằng:

```text
SPSC queue
Command queue
Snapshot
```

không chia sẻ mutable state.

Đây là architecture nên hướng tới.

---

# 105. Target architecture cuối cùng

```text
                       CONTROL PLANE
                              │
             ┌────────────────┼────────────────┐
             │                │                │
            UI               MPV          System Device
             │                │                │
             └────────────────┼────────────────┘
                              ▼
                    AudioCommandQueue
                              │
                              ▼
                    ┌─────────────────┐
                    │  AudioRuntime   │
                    │ State/Epoch     │
                    └───────┬─────────┘
                            │
                ┌───────────┼────────────┐
                │           │            │
                ▼           ▼            ▼
             Capture     Processor      Output
             Thread       Thread        Thread
                │           │            │
                ▼           ▼            ▼
             Raw SPSC   Process SPSC  DeviceManager
                │           │            │
                │           │      ┌─────┼─────┐
                │           │      ▼     ▼     ▼
                │           │    SDL   WASAPI ASIO
                │           │
                │           └──────► Analyzer
                │
                └──────────────► Recorder/STT/etc.
```

---

# 106. Phase refactor đề xuất

## PHASE 0 — Freeze architecture

Không thêm feature mới.

Mục tiêu:

```text
Không thay đổi behavior lớn.
Chỉ audit.
```

---

## PHASE 1 — Sửa Thread Ownership

Ưu tiên cao nhất.

Sửa:

```text
Audio.cpp
SpscRingBuffer.h
AudioProcessor.*
AudioOutputWorker.*
```

Mục tiêu:

```text
Không thread nào ngoài Consumer được đọc queue.
```

Đặc biệt bỏ:

```cpp
Audio::FlushProcessedBuffer()
```

khỏi UI path.

---

## PHASE 2 — AudioCommandQueue

Thêm:

```text
AudioCommand
AudioCommandQueue
AudioRuntimeState
AudioEpoch
```

Seek:

```text
UI → CommandQueue
```

Không trực tiếp thao tác buffer.

---

## PHASE 3 — Control Snapshot

Tách:

```text
PlayerStateSystem
```

khỏi Audio Worker.

Thêm:

```text
AudioControlSnapshot
```

PlayerState → Snapshot.

Audio Worker chỉ đọc snapshot.

---

## PHASE 4 — Device Manager

Tách:

```text
AudioOutputWorker
```

thành:

```text
AudioOutputWorker
AudioDeviceManager
IAudioOutputDevice
```

Output Worker không tự quản lý toàn bộ recovery.

---

## PHASE 5 — Driver Registry

Thay:

```cpp
switch(AudioBackendType)
```

bằng:

```text
AudioDriverRegistry
```

Thêm:

```text
SDL
WASAPI
Null
```

ASIO sau.

---

## PHASE 6 — Latency Engine

Thêm:

```text
AudioLatencyConfig
AudioClock
BufferPolicy
```

Loại bỏ hardcode:

```text
25ms
32 blocks
sleep 1ms
```

---

## PHASE 7 — Format Pipeline

Thêm:

```text
FormatNegotiator
Resampler
FormatConverter
```

---

## PHASE 8 — Recovery

Thêm:

```text
DeviceLost
Recovery
Backoff
Reopen
Reconfigure
```

---

## PHASE 9 — AudioBlock Pool

Chỉ làm sau khi architecture ổn định.

Mục tiêu:

```text
zero allocation
zero copy hoặc minimal copy
```

---

## PHASE 10 — Analyzer

Tách:

```text
RMS
Peak
FFT
Spectrogram
STT
```

khỏi realtime playback path.

---

# 107. Thứ tự file nên sửa

## Nhóm 1 — Không được sửa đồng thời

### `SpscRingBuffer.h`

Đây là nền móng.

Phải ổn định trước.

---

### `Audio.cpp`

Sau khi queue contract ổn định.

---

### `AudioProcessor.*`

Sau Audio.cpp/Queue.

---

### `AudioOutputWorker.*`

Sau Processor.

---

## Nhóm 2

```text
IAudioOutputDevice.h
SdlAudioDevice.*
```

sau khi ownership model rõ ràng.

---

## Nhóm 3

```text
AudioCaptureManager.*
```

sau khi generation/format model ổn định.

---

## Nhóm 4

Thêm mới:

```text
AudioRuntime
AudioCommand
AudioClock
AudioDeviceManager
AudioDriverRegistry
AudioControlSnapshot
```

---

# 108. Các file hiện tại cần giữ ổn định

Không nên cùng lúc refactor:

```text
Capture
Processor
Output
Driver
RingBuffer
```

vì nếu có bug sẽ không biết regression đến từ đâu.

Nên:

```text
Phase 1:
RingBuffer + queue ownership

Phase 2:
Control commands

Phase 3:
Output lifecycle

Phase 4:
Capture format

Phase 5:
Driver
```

---

# 109. Test matrix bắt buộc

## Thread safety

- Start/Stop liên tục.
- Start trong khi Stop.
- Seek spam.
- Track change spam.
- Seek + shutdown.
- Switch backend + seek.
- Device disconnect + seek.
- Device reconnect + pause.
- Pause/resume spam.

---

# 110. Stress test

```text
10,000 seek
10,000 pause/resume
100 backend switches
100 device disconnect/reconnect
```

Trong khi:

```text
video playing
audio active
visualizer active
```

Không được:

```text
deadlock
crash
audio corruption
memory growth
```

---

# 111. Long-run test

Chạy:

```text
6–24 giờ
```

theo dõi:

```text
RSS memory
CPU
queue occupancy
dropped blocks
underruns
device recovery
thread count
handle count
```

Memory phải:

```text
stable
```

không tăng theo thời gian.

---

# 112. Deadlock test

Không để Audio thread gọi ngược:

```text
Audio
 ↓
PlayerStateSystem
 ↓
some mutex
 ↓
Audio
```

Đây là dạng lock inversion rất nguy hiểm.

Sau refactor:

```text
PlayerStateSystem
 ↓
AudioControlSnapshot
```

một chiều.

---

# 113. Dependency rule

Audio:

```text
may depend on:
    platform adapter
    driver interface

should not depend on:
    UI
    WindowManager
    ImGui
    PlayerState internal locks
```

MPV adapter cũng phải nằm ngoài core.

---

# 114. Mục tiêu performance

Mục tiêu thực tế:

```text
Audio output latency:
10–30 ms

CPU:
<1–3% cho pipeline cơ bản

Allocation:
0 trong realtime path

Mutex:
0 trong realtime path

Copy:
1 lần hoặc tốt hơn

Queue:
bounded

Memory:
constant

Recovery:
automatic

Seek:
no stale audio
```

Các con số này còn phụ thuộc driver/hardware, nhưng đây nên là target architecture.

---

# 115. Mục tiêu thread model

Cuối cùng nên có khoảng:

```text
AudioControlThread
AudioCaptureThread
AudioProcessorThread
AudioOutputThread
```

Không tạo thread cho mọi thứ.

Analyzer nặng chỉ tạo thread riêng nếu thực sự cần.

---

# 116. Thread model tối ưu

```text
Control
   │
   ▼
CommandQueue
   │
   ├───────────────┐
   ▼               ▼
Capture         Output
   │               ▲
   ▼               │
Processor ─────────┘
```

Nếu analyzer nặng:

```text
Processor
    │
    ▼
AnalyzerQueue
    │
    ▼
AnalyzerThread
```

---

# 117. Điều quan trọng nhất

Không nên cố giải quyết thread safety bằng cách:

```text
thêm mutex
```

Mà phải giải quyết bằng:

```text
ownership
```

Nguyên tắc:

```text
ONE RESOURCE
      ↓
ONE OWNER
      ↓
ONE MUTATION THREAD
```

Các thread khác:

```text
command
snapshot
atomic metric
```

Đây là kiến trúc phù hợp nhất cho Audio Runtime.

---

# 118. Kết luận audit

Hệ thống hiện tại **không cần viết lại từ đầu**.

Nền móng đang khá tốt.

Các thành phần nên giữ:

```text
AudioBlock
SPSC RingBuffer
Capture Worker
Processor Worker
Output Worker
IAudioOutputDevice
Generation concept
Fixed-size PCM storage
Named Pipe
```

Nhưng cần thay đổi ownership/lifecycle.

Đặc biệt 5 việc phải làm trước mọi tối ưu khác:

```text
1. Không cho UI flush SPSC queue.
2. Tách Control Plane khỏi Audio Data Plane.
3. Tách PlayerStateSystem khỏi realtime worker.
4. Đưa Device lifetime vào single-owner DeviceManager.
5. Giảm buffer/latency và loại bỏ polling không cần thiết.
```

Sau đó mới:

```text
6. Multi-driver
7. Format negotiation
8. Resampler
9. Auto recovery
10. AudioBlockPool
11. SIMD
12. Advanced analyzer
```

---

# 119. Kiến trúc mục tiêu cuối cùng

```text
                         ┌────────────────────────┐
                         │     PLAYER / UI / OS   │
                         └────────────┬───────────┘
                                      │
                                      ▼
                         ┌────────────────────────┐
                         │  AudioControlPlane     │
                         │  CommandQueue          │
                         └────────────┬───────────┘
                                      │
                                      ▼
                         ┌────────────────────────┐
                         │      AudioRuntime      │
                         │                        │
                         │ State / Epoch / Clock  │
                         │ Snapshot / Metrics     │
                         └────────────┬───────────┘
                                      │
                ┌─────────────────────┼─────────────────────┐
                │                     │                     │
                ▼                     ▼                     ▼
       ┌────────────────┐    ┌────────────────┐    ┌────────────────┐
       │ AudioCapture   │    │ AudioProcessor │    │ AudioOutput    │
       │ Thread         │    │ Thread         │    │ Thread         │
       └───────┬────────┘    └───────┬────────┘    └───────┬────────┘
               │                     │                     │
               ▼                     ▼                     ▼
          Raw SPSC              Processed SPSC       DeviceManager
               │                     │                     │
               │                     │          ┌──────────┼─────────┐
               │                     │          │          │         │
               │                     │          ▼          ▼         ▼
               │                     │         SDL       WASAPI    ASIO
               │                     │
               │                     └──────────► Analyzer
               │
               └────────────────────────────────► Recorder
                                                    STT
                                                    TTS
```

Đây là kiến trúc có thể phát triển từ **media player hiện tại** thành một **Audio Runtime độc lập**, thay vì chỉ là một module phụ thuộc vào MPV.

---

# 120. Ưu tiên triển khai thực tế

Nếu bắt đầu refactor ngay bây giờ, thứ tự tôi khuyến nghị là:

```text
P0
│
├── Sửa violation SPSC
│
├── Cấm external queue mutation
│
└── Đảm bảo Shutdown/Start không race
        │
        ▼
P1
│
├── AudioCommandQueue
├── AudioEpoch
└── AudioRuntimeState
        │
        ▼
P2
│
├── AudioControlSnapshot
├── bỏ PlayerStateSystem khỏi OutputLoop
└── state transition
        │
        ▼
P3
│
├── AudioDeviceManager
├── device ownership
└── transactional backend switch
        │
        ▼
P4
│
├── WASAPI
├── SDL cleanup
├── Driver Registry
└── Null Driver
        │
        ▼
P5
│
├── AudioClock
├── latency controller
├── buffer policy
└── resync
        │
        ▼
P6
│
├── Format negotiation
├── Resampler
└── Converter
        │
        ▼
P7
│
├── BlockPool
├── zero allocation
├── zero/minimal copy
└── SIMD
        │
        ▼
P8
│
├── Analyzer
├── FFT
├── STT
└── Recording
```

## Verdict cuối cùng

**Kiến trúc hiện tại có thể cứu và nâng cấp, không cần rewrite toàn bộ.**

Điểm nguy hiểm nhất hiện tại không phải SDL, MPV hay Named Pipe mà là **ownership của SPSC queue và lifecycle giữa các thread**.

Nếu sửa đúng:

```text
SPSC ownership
+
Command Queue
+
Epoch/Generation
+
Control Snapshot
+
Single-owner Device
+
State Machine
```

thì phần lớn các vấn đề:

```text
deadlock
race
stale audio
seek corruption
device reconnect
backend switch
shutdown crash
```

sẽ được giải quyết ở cấp kiến trúc thay vì vá từng lỗi riêng lẻ.

Sau đó mới tối ưu:

```text
latency
CPU
memory
copy
SIMD
driver performance
```

Đây là hướng phù hợp nhất để Im_player có một Audio System đủ mạnh cho cả playback hiện tại và các nhu cầu sau này như **STT/subtitle, TTS, recording, audio analysis và nhiều backend audio khác nhau**.