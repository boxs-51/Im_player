# IM_PLAYER --- Phân tích toàn diện Audio Pipeline

## Audit mã nguồn `src(20260815-123434).zip`

### Ngày: 2026-08-15

> Phạm vi: phân tích trực tiếp source trong ZIP được cung cấp, tập trung
> vào toàn bộ đường đi Audio: MPV → Capture/Named Pipe → SPSC →
> Processor → Output → SDL, cùng với Seek/Track
> change/State/Filter/Lifecycle/Threading liên quan.

------------------------------------------------------------------------

# 1. Tóm tắt điều hành

Kiến trúc Audio hiện tại có hướng thiết kế đúng:

``` text
MPV
 │
 │ PCM f32le
 ▼
AudioCaptureManager
 │
 │ SPSC
 ▼
AudioProcessor
 │
 │ SPSC
 ▼
AudioOutputWorker
 │
 ▼
SDL Audio
```

Việc dùng **SPSC ring buffer** cho:

``` text
Capture → Processor
Processor → Output
```

là phù hợp với mô hình một producer / một consumer.

Tuy nhiên, implementation hiện tại **chưa đạt mức thread-safe
production-ready**. Các vấn đề quan trọng nhất không nằm ở thuật toán
SPSC cơ bản mà nằm ở **ownership, lifecycle, seek/generation, device
lifetime và việc nhiều thread can thiệp vào cùng resource**.

## Các lỗi mức Critical

1.  `Audio::OnUserSeek()` trực tiếp consume `m_processedAudioBuffer` từ
    UI/control thread, phá vỡ invariant SPSC.
2.  `AudioCaptureManager::NotifySeekOrTrackChange()` được khai báo nhưng
    không có implementation trong source hiện tại.
3.  `Audio::OnUserSeek()` không được tìm thấy ở call site nào;
    generation hiện tại vì vậy thực tế không được tăng khi seek.
4.  `AudioProcessor` dùng double-buffer visualizer nhưng chỉ atomic hóa
    index; dữ liệu `AudioVisualizerFrame` vẫn có data race.
5.  `AudioOutputWorker::SwitchBackend()` và `OutputLoop()` cùng truy cập
    `m_audioDevice`/`m_currentBackendType`; ownership device chưa thuộc
    riêng OutputThread.
6.  `SdlAudioDevice::Close()` dùng atomic device ID nhưng không bảo vệ
    lifetime của SDL device giữa
    `Write()/GetQueuedSizeBytes()/FlushBuffers()` và `Close()`.
7.  `AudioBlock::sample_count()` không giới hạn `channels`, trong khi
    storage chỉ đủ stereo; audio \>2 channels có thể dẫn tới
    out-of-bounds.
8.  `UpdateFormatCacheFromState()` dùng `sampleRate` và `channel_count`
    chưa khởi tạo nếu State chưa có dữ liệu.
9.  `AudioCaptureManager` cấu hình `ao`, `ao-pcm-file`, `ao-pcm-format`
    sau khi `Player::Init()` đã gọi `mpv_initialize()`. Đây là điểm
    thiết kế rất nghiêm trọng vì MPV options phải được cấu hình đúng
    lifecycle; return code hiện tại còn bị bỏ qua.
10. PTS của PCM block được lấy từ `time-pos` rồi tự cộng duration; đây
    không phải audio timestamp đáng tin cậy cho STT/subtitle
    synchronization.
11. Named pipe là byte stream nhưng code coi mỗi `ReadFile()` như một
    AudioBlock hoàn chỉnh; không có PCM accumulator để đảm bảo frame
    boundary.
12. `AudioOutputWorker` có logic drop block khi queue \>25 và thêm sleep
    polling, tạo discontinuity và latency khó dự đoán.
13. Shutdown reset processed ring từ bên ngoài consumer thread; điều này
    trái với invariant SPSC.
14. SDL subsystem được Init/Quit bên trong từng device object; ownership
    global của SDL audio subsystem chưa rõ ràng.

## Đánh giá tổng thể

  Hạng mục                          Đánh giá
  ------------------------------- ----------
  Ý tưởng kiến trúc                     8/10
  SPSC ring implementation            7.5/10
  Thread ownership                      4/10
  Seek/generation                       3/10
  Device lifetime                       4/10
  PCM framing                           4/10
  PTS/AV sync                           4/10
  Shutdown lifecycle                    5/10
  Latency design                        5/10
  Khả năng mở rộng STT/subtitle         6/10
  Production readiness              **5/10**

------------------------------------------------------------------------

# 2. Kiến trúc thực tế trong source

Các file Audio chính:

``` text
src/player/audio/
├── Audio.h / Audio.cpp
├── AudioTypes.h
├── AudioCaptureManager.h / .cpp
├── AudioProcessor.h / .cpp
├── AudioOutputWorker.h / .cpp
├── SpscRingBuffer.h
├── IAudioOutputDevice.h
├── SdlAudioDevice.h / .cpp
├── WasapiAudioDevice.h
├── AudioVisualizerData.h
├── audio_capture_engine.h / .cpp
├── ring_buffer.h
└── filter/
    ├── af_m.h
    ├── af_m_core.cpp
    ├── af_m_engine.cpp
    ├── af_m_dispatcher.cpp
    ├── af_m_io.cpp
    ├── af_m_context.cpp
    ├── af_m_analyzer.cpp
    ├── af_m_ai.cpp
    ├── af_m_safety.cpp
    └── af_m_sync.cpp
```

Pipeline chính:

``` text
PlayerSession
    │
    ├── Player / mpv_handle
    │
    ├── PlayerStateSystem
    │
    └── Audio
          │
          ├── AudioCaptureManager
          │       │
          │       └── raw SPSC
          │
          ├── AudioProcessor
          │       │
          │       └── processed SPSC
          │
          └── AudioOutputWorker
                  │
                  └── SdlAudioDevice
```

Ngoài ra còn một `AudioFilterManager` điều khiển MPV `af`.

------------------------------------------------------------------------

# 3. Ownership model hiện tại

Ý định của code là:

``` text
CaptureThread
    producer
       ↓
RawSPSC
    consumer
ProcessorThread
    producer
       ↓
ProcessedSPSC
    consumer
OutputThread
```

Đây là mô hình đúng.

Nhưng code thực tế có thêm các đường truy cập:

``` text
UI/control
   │
   └── Audio::OnUserSeek()
            │
            └── processedSPSC.acquire_read()
```

và:

``` text
UI/control
   │
   └── AudioOutputWorker::SwitchBackend()
            │
            └── m_audioDevice
```

Do đó ownership thực tế trở thành:

``` text
             ┌── Processor
             │
ProcessedSPSC┼── Output
             │
             └── UI   <-- SAI
```

và:

``` text
             ┌── OutputThread
m_audioDevice┤
             └── Control/UI <-- SAI
```

Đây là nguyên nhân nền tảng của nhiều race.

------------------------------------------------------------------------

# 4. Phân tích `SpscRingBuffer`

## 4.1 Phần memory ordering

Code sử dụng pattern:

``` cpp
producer:
    head.load(relaxed)
    tail.load(acquire)
    write data
    head.store(release)

consumer:
    tail.load(relaxed)
    head.load(acquire)
    read data
    tail.store(release)
```

Đây là pattern phù hợp với SPSC.

`alignas(64)` cho head/tail cũng là lựa chọn hợp lý để giảm false
sharing.

## 4.2 Vấn đề lớn không nằm ở atomic

Atomic index không cho phép biến SPSC thành MPSC/MPMC.

Ví dụ:

``` text
Processor:
    release_read()

UI:
    release_read()
```

không được phép chỉ vì `m_tail` là atomic.

Atomic chỉ đảm bảo thao tác trên index không bị torn; nó không bảo vệ
protocol ownership.

## 4.3 `commit_write()` dễ bị misuse

API:

``` cpp
T* acquire_write();
void commit_write();
```

không lưu trạng thái "slot đang được acquire".

Nếu code tương lai gọi:

``` cpp
acquire_write();
acquire_write();
commit_write();
```

hoặc commit sai lifecycle thì buffer có thể bị corrupt.

Khuyến nghị thêm debug invariant:

``` cpp
assert(!writeAcquired);
writeAcquired = true;
```

và:

``` cpp
assert(writeAcquired);
writeAcquired = false;
```

Tương tự cho consumer.

## 4.4 `size()/occupancy()` chỉ nên coi là snapshot

Các hàm này dùng relaxed load:

``` cpp
head.load(relaxed)
tail.load(relaxed)
```

Điều này có thể chấp nhận cho metric/heuristic.

Không được dùng occupancy để quyết định ownership hoặc correctness.

------------------------------------------------------------------------

# 5. Critical: `Audio::OnUserSeek()` phá vỡ SPSC

Code hiện tại:

``` cpp
void Audio::OnUserSeek() {
    m_audioCapture.NotifySeekOrTrackChange();

    const AudioBlock* slot = nullptr;
    while ((slot = m_processedAudioBuffer.acquire_read()) != nullptr) {
        m_processedAudioBuffer.release_read();
    }
}
```

`m_processedAudioBuffer` có:

``` text
Producer = AudioProcessor
Consumer = AudioOutputWorker
```

nhưng `OnUserSeek()` thêm consumer thứ ba.

Đây là **SPSC violation**.

Hậu quả:

-   mất block;
-   skip block;
-   generation sai;
-   audio cũ;
-   audio duplicate;
-   queue state không còn predictable;
-   race trong head/tail protocol;
-   lỗi có thể chỉ xuất hiện dưới seek spam.

## Cách đúng

UI chỉ gửi command:

``` text
UI
 ↓
Seek command
 ↓
generation request
 ↓
Capture/Processor
 ↓
Output
```

Không UI nào được `acquire_read()` ring của Output.

------------------------------------------------------------------------

# 6. Critical: `NotifySeekOrTrackChange()` chưa được implement

Header có:

``` cpp
void NotifySeekOrTrackChange();
```

nhưng source `AudioCaptureManager.cpp` hiện tại không có definition
tương ứng.

Nếu hàm được link từ `Audio.cpp` thì có nguy cơ:

``` text
LNK2019 unresolved external symbol
```

Đây phải được sửa ngay.

------------------------------------------------------------------------

# 7. Critical: Seek hiện tại thực tế chưa kích hoạt generation

Tìm toàn bộ source cho:

``` text
OnUserSeek(
```

chỉ thấy declaration/implementation, không thấy call site thực tế.

Trong `PlaybackCommand::DoSeek()`:

``` cpp
const char* cmd[] = { "seek", buffer, "absolute", nullptr };
int ret = Exec(cmd);
```

không gọi:

``` cpp
Audio::OnUserSeek()
```

Trong `PlaybackObserver`:

``` cpp
MPV_EVENT_SEEK
```

chỉ log.

Vì vậy:

``` text
m_activeGeneration
```

được reset về 0 lúc StartCapture và không có cơ chế tăng generation thực
tế.

Trong khi OutputWorker lại dựa vào:

``` cpp
block->generation
```

để bỏ audio cũ.

=\> Cơ chế chống stale audio hiện tại **chưa hoạt động như thiết kế**.

------------------------------------------------------------------------

# 8. Thiết kế generation đúng

Không cho UI sửa:

``` cpp
m_activeGeneration
```

CaptureThread phải là owner.

Đề xuất:

``` cpp
std::atomic<uint64_t> m_generationRequest{0};
```

Control:

``` cpp
void RequestDiscontinuity() {
    m_generationRequest.fetch_add(1, std::memory_order_acq_rel);
}
```

CaptureThread:

``` cpp
uint64_t requested =
    m_generationRequest.load(std::memory_order_acquire);

if (requested != m_activeGeneration) {
    m_activeGeneration = requested;
    m_sequence = 0;
    m_currentPts = 0.0;
}
```

Sau đó mọi block mới mang:

``` cpp
block.generation = m_activeGeneration;
```

Output chỉ nhận/discard theo generation.

------------------------------------------------------------------------

# 9. Cần phân biệt Seek Request và Media Generation

Không nên chỉ tăng generation khi UI nhấn seek.

Generation nên tăng cho mọi discontinuity:

``` text
Seek
Track change
Loadfile
Playlist next
Playlist previous
Audio track change
Device reconfiguration
MPV audio reconfiguration
EOF → new media
```

Mô hình:

``` cpp
enum class AudioDiscontinuityReason {
    Seek,
    TrackChange,
    MediaLoad,
    AudioReconfig,
    DeviceReset,
    Shutdown
};
```

------------------------------------------------------------------------

# 10. Critical: Visualizer double-buffer vẫn data race

Code:

``` cpp
int activeIdx = m_writeIndex.load(...);
outFrame = m_visualizerFrames[activeIdx];
```

Producer:

``` cpp
AudioVisualizerFrame& frame =
    m_visualizerFrames[targetIdx];

frame.rmsLeft = ...;
frame.rmsRight = ...;
...
m_writeIndex.store(targetIdx, release);
```

Vấn đề:

``` text
UI đọc Frame A
        │
        ├── copy sequence
        ├── copy rms
        │
Processor bắt đầu ghi lại Frame A
        ├── ghi spectrum
        └── ghi peak
```

=\> data race.

Atomic index chỉ bảo vệ việc publish index, không bảo vệ lifetime của
frame.

## Giải pháp đơn giản nhất

Visualizer không phải audio realtime output path.

Dùng mutex nhỏ:

``` cpp
std::mutex m_visualizerMutex;
AudioVisualizerFrame m_visualizerFrame;
```

Producer:

``` cpp
std::lock_guard lock(m_visualizerMutex);
m_visualizerFrame = newFrame;
```

UI:

``` cpp
std::lock_guard lock(m_visualizerMutex);
outFrame = m_visualizerFrame;
```

Độ trễ không đáng kể.

------------------------------------------------------------------------

# 11. Critical: `AudioBlock` có nguy cơ overflow khi channels \> 2

Storage:

``` cpp
constexpr size_t kMaxAudioChannels = 2;
constexpr size_t kMaxAudioSamples = 4096;
std::array<float, 4096> samples;
```

nhưng:

``` cpp
size_t sample_count() const {
    return frames * format.channels;
}
```

Nếu:

``` text
frames = 2048
channels = 6
```

thì:

``` text
sample_count = 12288
```

trong khi storage chỉ:

``` text
4096
```

Output:

``` cpp
for (i < sampleCount)
```

có thể đọc vượt mảng.

Processor cũng truy cập:

``` cpp
block.samples[i * channels]
```

với channels lớn.

## Nếu pipeline chỉ stereo

Phải enforce:

``` cpp
if (channels != 1 && channels != 2)
    reject;
```

Tốt hơn nữa:

``` text
MPV
 ↓
normalize
 ↓
F32 / stereo / 48k
```

------------------------------------------------------------------------

# 12. Critical: uninitialized format cache

Code:

``` cpp
int sampleRate;
int channel_count;
```

sau đó chỉ assign nếu:

``` cpp
if (model.params.asamplerate)
```

và:

``` cpp
if (model.params.channel_count)
```

Nếu State chưa có audio parameters:

``` text
sampleRate = garbage
channel_count = garbage
```

Sau đó:

``` cpp
m_formatCache.sampleRate = sampleRate;
m_formatCache.channels = channel_count;
```

=\> Undefined Behavior.

Sửa:

``` cpp
int sampleRate = 48000;
int channel_count = 2;
```

và clamp:

``` cpp
sampleRate = std::clamp(sampleRate, 8000, 192000);
channel_count = std::clamp(channel_count, 1, 2);
```

------------------------------------------------------------------------

# 13. Critical: MPV options được set sai lifecycle

`Player::Init()`:

``` cpp
m_mpv = mpv_create();

ApplyStaticMPVConfig(m_mpv);

mpv_initialize(m_mpv);
```

Sau đó `PlayerSession::Init()` mới:

``` cpp
m_audio->Init(m_player->GetHandle(), m_state.get());
```

và `AudioCaptureManager::Init()` gọi:

``` cpp
mpv_set_option_string(m_mpv, "ao", "pcm");
mpv_set_option_string(m_mpv, "ao-pcm-file", m_pipeName.c_str());
mpv_set_option_string(m_mpv, "ao-pcm-format", "f32le");
```

Điểm này rất đáng lo.

Các MPV **options** phải được cấu hình đúng trước `mpv_initialize()`.

Trong code hiện tại return code của:

``` cpp
mpv_set_option_string()
```

không được kiểm tra.

Do đó nếu MPV từ chối option sau initialize, AudioCapture vẫn báo Init
thành công.

## Cách sửa

Đưa các static audio options vào `ApplyStaticMPVConfig()` trước:

``` cpp
mpv_initialize()
```

Nhưng `ao-pcm-file` lại phụ thuộc pipe name của `AudioCaptureManager`.

Do đó tốt nhất tách lifecycle:

``` text
Create AudioCaptureConfig
       ↓
Create pipe name
       ↓
Configure MPV options
       ↓
mpv_initialize
       ↓
start session
```

Không để `AudioCaptureManager` tự cấu hình option sau khi MPV đã
initialize.

------------------------------------------------------------------------

# 14. Named Pipe architecture

Hiện tại:

``` text
AudioCaptureThread
    CreateNamedPipe
       ↓
    ConnectNamedPipe
       ↓
    ReadFile
```

ý tưởng hợp lý.

Nhưng có một vấn đề quan trọng:

``` text
Named pipe = byte stream
```

Một:

``` cpp
ReadFile()
```

không đảm bảo trả đúng:

``` text
N frames
```

Code hiện tại:

``` cpp
bytesRead
 ↓
sampleCount = bytesRead / sizeof(float)
 ↓
frames = sampleCount / channels
```

Nếu bytesRead không chia hết cho frame size:

``` text
1 sample/frame fragment
```

có thể bị bỏ.

Đây là nguồn discontinuity.

------------------------------------------------------------------------

# 15. Cần PCM accumulator

Đề xuất:

``` text
ReadFile()
   ↓
byte accumulator
   ↓
đủ frame size?
   │
   ├── no → đọc tiếp
   │
   └── yes
         ↓
      AudioBlock
```

Ví dụ stereo float32:

``` text
1 frame = 2 * sizeof(float) = 8 bytes
```

Nếu block target = 960 frames:

``` text
blockBytes = 960 * 8 = 7680
```

Chỉ commit block khi đủ 7680 bytes.

------------------------------------------------------------------------

# 16. `AudioCaptureManager` đang trộn capture và timing

Trong CaptureLoop:

``` cpp
m_stateSystem->ReadPlayback(
    [&timepos](const PlaybackModel& model) {
        timepos = model.timing.timePos;
    }
);
```

nhưng `timepos` chưa được initialize trước lambda.

Nếu state callback không gán vì logic thay đổi sau này:

``` text
timepos = garbage
```

Nên:

``` cpp
double timepos = 0.0;
```

------------------------------------------------------------------------

# 17. PTS hiện tại không phải audio PTS thực

Code:

``` cpp
m_currentPts = timepos;
```

sau đó:

``` cpp
m_currentPts += blockDuration;
```

Có nghĩa:

``` text
block PTS
=
snapshot time-pos
+
ước lượng duration
```

Điều này không đảm bảo đúng audio sample timeline.

Sai lệch có thể xảy ra khi:

``` text
seek
pause
speed change
audio delay
buffering
decoder delay
network jitter
AO buffering
```

Đặc biệt nếu mục tiêu tương lai là:

``` text
Audio
 ↓
STT
 ↓
Subtitle
```

thì timestamp này chưa đủ tin cậy.

------------------------------------------------------------------------

# 18. `AudioOutputWorker` đang làm AV sync theo một snapshot không phù hợp

Code:

``` cpp
double targetPTS = currentPTS + audioDelay;
double drift = abs(block->pts - targetPTS);
```

và:

``` cpp
if (drift > 0.3)
    drop block;
```

Có hai vấn đề.

### Một

`currentPTS` là trạng thái playback snapshot.

### Hai

Block PTS đã được tạo bằng một snapshot `time-pos` trước đó.

Hai clock:

``` text
MPV playback clock
Audio block clock
```

không được thiết kế thành một `AudioClock`.

Kết quả là:

``` text
drift > 0.3
```

có thể drop audio quá mạnh.

------------------------------------------------------------------------

# 19. Không nên dùng `time-pos` để "gắn timestamp" từng ReadFile

Nếu muốn sync chính xác:

``` text
audio sample position
```

phải là clock cơ sở.

Ví dụ:

``` cpp
AudioClock {
    uint64_t sampleIndex;
    double startPts;
    uint32_t sampleRate;
};
```

Block:

``` cpp
pts = clock.startPts +
      sampleIndex / sampleRate;
```

Seek:

``` text
new generation
new sample epoch
new startPts
```

------------------------------------------------------------------------

# 20. Critical: `SdlAudioDevice` lifetime race

Hiện tại:

``` cpp
Write()
    load m_deviceId
    SDL_QueueAudio()
```

trong khi:

``` cpp
Close()
    m_deviceId.exchange(0)
    SDL_CloseAudioDevice()
```

Atomic device ID không đảm bảo:

``` text
load handle
 ↓
resource remains alive
```

Ví dụ:

``` text
Thread A                  Thread B

Write()
deviceId = 10

                          Close()
                          deviceId = 0
                          SDL_CloseAudioDevice(10)

SDL_QueueAudio(10)
```

=\> use-after-close ở resource level.

------------------------------------------------------------------------

# 21. Đừng dùng atomic handle để giải quyết ownership

Giải pháp tốt:

``` text
OutputThread
    owns SdlAudioDevice
       │
       ├── Open
       ├── Write
       ├── GetQueued
       ├── Flush
       ├── Recover
       └── Close
```

Control thread chỉ gửi:

``` text
SwitchBackend
```

vào command queue.

------------------------------------------------------------------------

# 22. Critical: `SwitchBackend()` hiện tại phá ownership

``` cpp
bool AudioOutputWorker::SwitchBackend(...)
```

gọi:

``` cpp
Stop();
m_audioDevice = CreateDeviceBackend(...);
m_audioDevice->Open(...);
Start();
```

Nếu được gọi từ UI thread, device được tạo và thay đổi từ UI thread.

Trong khi OutputLoop có thể đang dùng device.

Hiện tại Stop join trước khi thay device nên một số race có thể được
tránh trong đúng call path này, nhưng thiết kế vẫn nguy hiểm vì:

-   `OutputLoop` cũng tự recovery và thay `m_audioDevice`;
-   `m_currentBackendType` bị đọc/ghi khác thread;
-   `m_audioDevice` không có một owner thread duy nhất;
-   các API public có thể được gọi đồng thời.

## Kiến trúc nên là

``` text
UI
 ↓
CommandQueue
 ↓
OutputThread
 ↓
SwitchBackendInternal()
```

------------------------------------------------------------------------

# 23. Critical: Auto recovery cũng thay `unique_ptr` ngay trong OutputThread

Điểm này tốt hơn `SwitchBackend()` vì nằm trong OutputLoop.

Nhưng vẫn có vấn đề thiết kế:

``` cpp
m_audioDevice->Shutdown();
m_audioDevice = CreateDeviceBackend(...);
```

trong cùng thread là hợp lý.

Điều cần làm là đảm bảo **mọi lifecycle device đều chỉ ở OutputThread**,
bao gồm cả Stop/Close.

Hiện `Stop()` vẫn gọi:

``` cpp
m_audioDevice->Close();
```

từ thread gọi Stop sau khi join.

Nên chuyển `Close()` vào chính OutputThread hoặc có một shutdown
command.

------------------------------------------------------------------------

# 24. SDL subsystem ownership

`SdlAudioDevice::Open()`:

``` cpp
SDL_InitSubSystem(SDL_INIT_AUDIO)
```

`Close()`:

``` cpp
SDL_QuitSubSystem(SDL_INIT_AUDIO)
```

Đây là resource global.

Nếu tương lai có:

``` text
Device A
Device B
```

A đóng có thể quit subsystem trong khi B còn dùng.

Nên ownership:

``` text
Application AudioRuntime
    SDL_InitSubSystem(SDL_INIT_AUDIO)
             │
             ├── SdlDevice A
             └── SdlDevice B
```

hoặc đảm bảo chỉ có một AudioRuntime/device.

------------------------------------------------------------------------

# 25. `obtainedSpec` bị bỏ qua

Open:

``` cpp
SDL_AudioSpec desiredSpec;
SDL_AudioSpec obtainedSpec;
SDL_OpenAudioDevice(...);
```

Nhưng pipeline vẫn giả định:

``` text
48k
float32
stereo
```

Nếu SDL thực tế trả format khác, output không còn được validate.

Nên lưu:

``` cpp
struct AudioDeviceFormat {
    uint32_t sampleRate;
    uint8_t channels;
    SDL_AudioFormat format;
};
```

và có conversion/resampler nếu cần.

------------------------------------------------------------------------

# 26. Pipeline format nên được chuẩn hóa

Khuyến nghị:

``` text
MPV
 ↓
Float32
 ↓
Stereo
 ↓
48kHz
 ↓
AudioCapture
```

Nếu MPV source là:

``` text
44.1k
mono
5.1
24-bit
```

thì normalize trước khi đưa vào pipeline nội bộ.

Điều này giúp:

-   fixed block size;
-   fixed sample_count;
-   đơn giản visualizer;
-   đơn giản STT;
-   đơn giản SDL/WASAPI;
-   tránh overflow.

------------------------------------------------------------------------

# 27. `AudioBlock` nên có validation

Đề xuất:

``` cpp
bool is_valid() const noexcept {
    if (frames == 0)
        return false;

    if (frames > kMaxAudioFrames)
        return false;

    if (format.sampleRate == 0)
        return false;

    if (format.channels == 0 ||
        format.channels > kMaxAudioChannels)
        return false;

    if (sample_count() > kMaxAudioSamples)
        return false;

    return true;
}
```

Mọi stage có thể dùng:

``` text
Capture → validate
Processor → assert/debug
Output → validate
```

------------------------------------------------------------------------

# 28. Bug logic: `std::array::empty()` không kiểm tra audio

Output:

``` cpp
if (block->samples.empty() || block->sample_count() == 0)
```

`std::array<float, 4096>` có size cố định nên:

``` cpp
block->samples.empty()
```

luôn false.

Điều cần kiểm tra là:

``` cpp
frames == 0
```

và:

``` cpp
sample_count() > 0
```

------------------------------------------------------------------------

# 29. Processor copy không thực sự zero-copy

Code comment:

``` text
ZERO-COPY WRITE
```

nhưng:

``` cpp
*outSlot = *inSlot;
```

là full struct copy.

Với:

``` text
4096 floats
```

mỗi block là khoảng:

``` text
16 KB
```

Ở 2048 frames / 48kHz:

``` text
~23.4 block/s
```

=\> khoảng:

``` text
~375 KB/s
```

chỉ cho một hướng copy samples.

Không phải bottleneck nghiêm trọng.

Nhưng comment nên sửa thành:

``` text
preallocated slot copy
```

không phải zero-copy.

------------------------------------------------------------------------

# 30. Latency hiện tại

Block:

``` text
2048 frames
```

ở 48k:

``` text
2048 / 48000
≈ 42.67 ms
```

Raw ring:

``` text
8 × 42.67
≈ 341 ms
```

Processed ring:

``` text
32 × 42.67
≈ 1.365 s
```

SDL desired buffer:

``` text
512 samples
≈ 10.67 ms
```

Output queue target:

``` text
25 ms
```

Nhưng processed ring có thể chứa hơn 1.3 giây.

Nếu mục tiêu:

``` text
realtime playback
STT
subtitle
visualizer
```

thì quá lớn.

------------------------------------------------------------------------

# 31. Drop strategy hiện tại

``` cpp
if (occupancy() > 25) {
    drop 5 blocks;
}
```

Mục tiêu là giảm latency.

Nhưng hậu quả:

``` text
sequence discontinuity
PTS discontinuity
audio discontinuity
```

Với playback realtime có thể chấp nhận.

Với STT thì rất xấu.

## Vì vậy phải tách Playback và Analysis

``` text
                 AudioCapture
                      │
                      ▼
                    Fanout
                  /         \
                 /           \
                ▼             ▼
        PlaybackRing      AnalysisRing
             │                 │
             ▼                 ▼
        AudioOutput           STT
```

------------------------------------------------------------------------

# 32. Kiến trúc cho mục tiêu Subtitle/STT

Không nên:

``` text
Raw SPSC
 ├── Output
 └── STT
```

vì SPSC chỉ cho một consumer.

Nên:

``` text
Capture
   │
   ▼
AudioFanout
   │
   ├── PlaybackRing
   │
   └── AnalysisRing
```

Mỗi ring:

``` text
one producer
one consumer
```

được giữ nguyên invariant.

------------------------------------------------------------------------

# 33. Pause hiện tại flush hardware liên tục

Code:

``` cpp
if (shouldSilence) {
    m_audioDevice->FlushBuffers();
    sleep(5ms);
    continue;
}
```

Khi pause:

``` text
Flush
sleep
Flush
sleep
Flush
...
```

Không cần.

Nên detect transition:

``` text
Playing → Paused
```

flush một lần.

Trong trạng thái:

``` text
Paused
Paused
Paused
```

chỉ chờ command/state change.

------------------------------------------------------------------------

# 34. Output loop đang polling

Có nhiều:

``` cpp
sleep_for(1ms)
sleep_for(5ms)
sleep_for(10ms)
sleep_for(2ms)
```

Polling tạo:

-   scheduling jitter;
-   CPU wakeup;
-   latency không deterministic.

Nên dùng:

``` text
condition_variable
```

cho command/data availability.

Tuy nhiên producer/consumer audio path vẫn có thể giữ SPSC lock-free;
condition variable chỉ dùng để wake thread khi ring transitions từ empty
→ non-empty.

------------------------------------------------------------------------

# 35. Backpressure hiện tại chưa nhất quán

Capture khi raw ring full:

``` text
sleep 2ms
```

Processor khi processed ring full:

``` text
if(outSlot) push
else silently drop
```

Output khi hardware queue full:

``` text
sleep 1ms
```

Ba tầng có policy khác nhau.

Nên định nghĩa rõ:

``` text
Capture:
    never block too long
    drop newest / oldest?

Processor:
    preserve playback continuity

Output:
    bound hardware queue
```

------------------------------------------------------------------------

# 36. Recommended drop policy

### Playback

Khi quá latency:

``` text
drop oldest queued block
```

để giữ realtime.

### Analysis/STT

Không nên drop nếu có thể tránh.

Nếu quá tải:

``` text
backpressure
```

hoặc dùng riêng analysis worker.

------------------------------------------------------------------------

# 37. Shutdown hiện tại

Thứ tự:

``` text
Output.Stop()
Processor.Stop()
Capture.Shutdown()
```

là đúng về dependency:

``` text
consumer
 ↓
processor
 ↓
producer
```

Nhưng sau đó:

``` cpp
Audio::Shutdown()
```

lại tự consume:

``` cpp
m_processedAudioBuffer.acquire_read()
```

Đây vẫn là SPSC violation.

Sau khi Output.Stop() thì về mặt thực tế không còn consumer thread,
nhưng ownership protocol vẫn bị phá.

## Đúng hơn

Ring được reset khi:

``` text
all producer/consumer stopped
```

và reset phải là lifecycle operation của pipeline, không phải UI tùy ý.

------------------------------------------------------------------------

# 38. Raw ring cũng cần reset

Shutdown hiện tại chỉ drain:

``` text
processed ring
```

không có reset raw ring.

Nếu object reuse hoặc re-init:

``` text
old raw block
    ↓
new Processor
```

có thể xuất hiện stale audio.

Cần:

``` text
RawRing.reset()
ProcessedRing.reset()
```

chỉ khi toàn bộ worker đã stopped.

------------------------------------------------------------------------

# 39. `AudioCaptureManager` lifecycle

`StartCapture()`:

``` cpp
if (m_isRunning) {
    m_isCapturing = true;
    return;
}
```

Nếu đang chạy nhưng cần reconfigure media:

``` text
format
pipe
generation
```

thì chỉ set capturing true không đủ.

Cần explicit:

``` text
Reconfigure
```

hoặc:

``` text
Discontinuity
```

------------------------------------------------------------------------

# 40. Named pipe reconnect

Loop reconnect:

``` text
CreateNamedPipe
ConnectNamedPipe
ReadFile
disconnect
close
retry
```

là hợp lý.

Nhưng khi pipe disconnect cần reset:

``` text
partial PCM accumulator
generation
PTS epoch
format
sequence
```

Nếu không, block đầu của stream mới có thể kế thừa state cũ.

------------------------------------------------------------------------

# 41. Overlapped I/O

Sử dụng:

``` cpp
FILE_FLAG_OVERLAPPED
OVERLAPPED
CancelIoEx
```

là đúng hướng.

Nhưng nên kiểm tra rõ các trường hợp:

``` text
ERROR_OPERATION_ABORTED
ERROR_BROKEN_PIPE
ERROR_PIPE_NOT_CONNECTED
ERROR_NO_DATA
ERROR_IO_PENDING
```

và phân biệt:

``` text
normal shutdown
pipe disconnect
actual error
```

Hiện tại log chung:

``` text
Pipe disconnected or stream read ended.
```

khó chẩn đoán.

------------------------------------------------------------------------

# 42. Pipe handle atomic chưa thực sự cần thiết

`m_atomicPipeHandle` được dùng để Stop thread gọi:

``` cpp
CancelIoEx(hPipe, NULL);
```

Điều này có thể giữ.

Nhưng cần đảm bảo:

``` text
StopCapture
    load handle
    CancelIoEx
```

không xảy ra sau khi CaptureThread đã:

``` text
exchange INVALID
CloseHandle
```

Windows handle reuse là một concern.

Cách an toàn hơn là dùng event shutdown riêng + owner thread quản lý
handle, hoặc lifecycle protocol rõ ràng.

------------------------------------------------------------------------

# 43. PlayerStateSystem

`PlayerStateSystem` dùng mutex riêng cho:

``` text
Playback
Audio
Video
...
```

Điểm này tốt.

Audio threads đọc:

``` cpp
ReadPlayback()
ReadAudio()
```

an toàn về memory.

Tuy nhiên snapshot giữa:

``` text
Playback
Audio
```

không atomic.

Ví dụ Output đọc:

``` text
Playback snapshot at T1
Audio snapshot at T2
```

có thể có state không cùng epoch.

Nếu cần consistency, tạo:

``` cpp
AudioRuntimeSnapshot
```

được publish atomically hoặc lấy trong một coordinated snapshot.

------------------------------------------------------------------------

# 44. `GetFullState()` dùng nhiều mutex

``` cpp
std::scoped_lock(
    playback,
    media,
    video,
    audio,
    ...
)
```

`std::scoped_lock` giúp tránh deadlock do lock ordering.

Điểm này tốt.

Nhưng AudioOutput không nên lấy full state vì quá nặng.

------------------------------------------------------------------------

# 45. AudioFilterManager

Filter manager điều khiển MPV `af`:

``` text
UI/Main
   ↓
AudioFilterManager
   ↓
mpv_set_property("af")
```

và:

``` text
UpdateAdaptiveFilters()
   ↓
mpv_command("af-command")
```

Đây là một control plane khác với PCM playback.

Không nên để AudioFilterManager can thiệp trực tiếp vào AudioOutput
device.

------------------------------------------------------------------------

# 46. Filter manager có vấn đề thread ownership

`AudioFilterManager` có nhiều mutable members:

``` text
m_filters
m_filterIndex
m_currentContext
m_currentPreset
m_autoMode
...
```

Nếu UI gọi:

``` text
ToggleFilter
UpdateParam
SetPreset
```

trong khi main loop gọi:

``` text
UpdateAdaptiveFilters
```

thì có nguy cơ data race nếu hai call path khác thread.

Hiện `main1.cpp` gọi `UpdateAdaptiveFilters()` từ main loop.

UI có thể gọi setter trực tiếp.

Cần xác định một owner thread hoặc thêm mutex/command queue.

------------------------------------------------------------------------

# 47. Filter manager và AudioCapture nên có boundary rõ

Hiện:

``` text
AudioFilterManager
    └── modifies MPV af

AudioCaptureManager
    └── captures MPV AO PCM
```

Điều này có nghĩa:

``` text
MPV audio filter chain
       ↓
PCM AO
       ↓
Named Pipe
```

Đây là hướng đúng nếu muốn capture **processed audio**.

Nhưng phải đảm bảo filter chain được áp dụng trước AO và capture format
sau filter vẫn đúng:

``` text
f32
stereo
48k
```

Nếu filter thay đổi channel layout/sample rate, CaptureManager phải
detect reconfiguration.

------------------------------------------------------------------------

# 48. `SetChannelMode("surround")` xung đột với AudioBlock stereo-only

Filter code có:

``` text
pan=5.1
```

nhưng:

``` cpp
kMaxAudioChannels = 2
```

Đây là xung đột kiến trúc rõ ràng.

Nếu user bật surround:

``` text
MPV output = 6 channels
```

nhưng AudioBlock chỉ chứa 2 channels.

Có nguy cơ overflow.

## Quyết định cần đưa ra

### Option A --- pipeline luôn stereo

Filter `surround` chỉ dành cho MPV hardware path khác.

### Option B --- AudioBlock dynamic/multi-channel

Phải thay storage.

Nếu mục tiêu STT/subtitle:

> Option A là phù hợp hơn.

------------------------------------------------------------------------

# 49. Visualizer spectrum hiện tại không phải FFT

Code:

``` cpp
frame.spectrum[b] =
    rms * factor;
```

đây là spectrum giả lập.

Nếu chỉ dùng UI:

``` text
██████
████
██
```

thì được.

Nếu muốn phân tích:

``` text
frequency
pitch
speech
voice activity
```

thì phải FFT/STFT.

------------------------------------------------------------------------

# 50. RMS calculation

RMS:

``` cpp
sqrt(sumSq / frames)
```

là đúng cơ bản.

Nhưng accumulation dùng:

``` cpp
float
```

nên có thể dùng:

``` cpp
double
```

cho độ ổn định số tốt hơn.

------------------------------------------------------------------------

# 51. Audio Output volume

Volume hiện tại:

``` cpp
volumeAdjustedBuffer.resize(sampleCount);
```

Nếu volume !=1:

``` text
copy + multiply
```

Không quá nặng.

Nhưng có thể preallocate:

``` cpp
std::array<float, kMaxAudioSamples>
```

để loại bỏ dynamic allocation khỏi audio loop.

------------------------------------------------------------------------

# 52. Hardware queue control

Code target:

``` text
25 ms
```

sau đó:

``` cpp
while (queued > target)
    sleep(1ms)
```

Điều này có thể hoạt động, nhưng latency thực tế:

``` text
MPV audio buffer
+
pipe
+
raw ring
+
processor
+
processed ring
+
SDL queue
```

không chỉ là SDL 25ms.

MPV static config còn:

``` text
audio-buffer = 0.5
```

=\> upstream có thể giữ 500ms.

Nếu mục tiêu low latency, cần audit toàn bộ buffer chain.

------------------------------------------------------------------------

# 53. MPV config hiện tại

`ApplyStaticMPVConfig()`:

``` text
audio-buffer = 0.5
audio-pitch-correction = yes
```

0.5 giây là khá lớn nếu mục tiêu:

``` text
realtime analysis
STT
subtitle
```

Playback local có thể chấp nhận.

Live low-latency thì cần profile riêng.

Nên có:

``` text
PlaybackProfile
AnalysisProfile
LiveProfile
```

thay vì một audio-buffer chung.

------------------------------------------------------------------------

# 54. Kiến trúc nên chuyển sang

## Data plane

``` text
                    MPV
                     │
                     ▼
             Audio Capture Thread
                     │
                     ▼
               RawAudioRing
                     │
                     ▼
             AudioProcessor
                     │
          ┌──────────┴──────────┐
          ▼                     ▼
   PlaybackAudioRing      AnalysisAudioRing
          │                     │
          ▼                     ▼
    OutputWorker                STT
          │                     │
          ▼                     ▼
       WASAPI/SDL             Subtitle
```

## Control plane

``` text
UI/Main
   │
   ▼
AudioCommandQueue
   │
   ├── Seek
   ├── TrackChange
   ├── Pause
   ├── Resume
   ├── Volume
   ├── SwitchBackend
   └── Shutdown
```

------------------------------------------------------------------------

# 55. Ownership model cuối cùng

## CaptureThread owns

``` text
Pipe
RawRing producer
PCM accumulator
generation state
sequence
audio clock
```

## ProcessorThread owns

``` text
RawRing consumer
PlaybackRing producer
AnalysisRing producer
DSP/FFT state
```

## OutputThread owns

``` text
PlaybackRing consumer
IAudioOutputDevice
SDL/WASAPI device
hardware queue
output clock
```

## AnalysisThread owns

``` text
AnalysisRing consumer
STT pipeline
subtitle timing
```

## UI/Main owns

``` text
commands only
visualizer snapshot only
```

------------------------------------------------------------------------

# 56. Seek protocol đề xuất

``` text
UI
 │
 │ seek(120.0)
 ▼
AudioCommandQueue
 │
 ▼
Control/MPV
 │
 ├── request MPV seek
 │
 └── generation++
          │
          ▼
     CaptureThread
          │
          ├── flush local PCM accumulator
          ├── reset sample epoch
          └── tag new blocks
                  generation=N+1
                         │
                         ▼
                    Processor
                         │
                         ▼
                    OutputWorker
                         │
                         ├── discard old generation
                         ├── clear hardware queue
                         └── accept new generation
```

Không có:

``` text
UI → RingBuffer.acquire_read()
```

------------------------------------------------------------------------

# 57. Track change protocol

``` text
TrackChange
    ↓
new generation
    ↓
flush capture partial bytes
    ↓
reset audio clock
    ↓
reset sequence
    ↓
new format detection
    ↓
new blocks
```

------------------------------------------------------------------------

# 58. Backend switching protocol

``` text
UI
 │
 ▼
AudioCommandQueue
 │
 ▼
OutputThread
 │
 ├── stop accepting new playback writes
 ├── drain/drop queued stale blocks
 ├── Close old backend
 ├── Create new backend
 ├── Open with pipeline format
 ├── reset hardware clock
 └── continue
```

------------------------------------------------------------------------

# 59. Shutdown protocol

``` text
Audio::Shutdown()
        │
        ▼
send Shutdown command
        │
        ▼
OutputThread stops
        │
        ▼
Processor stops
        │
        ▼
Capture stops
        │
        ▼
join all
        │
        ▼
reset RawRing
reset PlaybackRing
reset AnalysisRing
        │
        ▼
destroy devices
        │
        ▼
destroy Audio
```

Không thread ngoài tự drain SPSC.

------------------------------------------------------------------------

# 60. State machine đề xuất

``` cpp
enum class AudioPipelineState {
    Stopped,
    Starting,
    Running,
    Seeking,
    Reconfiguring,
    Paused,
    Error,
    Stopping
};
```

Không để mỗi class tự hiểu lifecycle khác nhau.

------------------------------------------------------------------------

# 61. Metrics nên bổ sung

Hiện có:

``` text
blocksReceived
blocksDropped
bytesReceived
ringOverflows
lastSequence
lastPTS
```

Nên thêm:

``` text
captureReadErrors
pipeReconnects
pipeDisconnects
partialFrames
rawRingHighWatermark
processedRingHighWatermark
outputDrops
staleGenerationDrops
ptsDrops
hardwareQueueBytes
deviceRecoveries
deviceOpenFailures
seekGeneration
captureLatency
outputLatency
```

Đặc biệt:

``` text
staleGenerationDrops
```

sẽ giúp chứng minh seek đang hoạt động.

------------------------------------------------------------------------

# 62. Logging nên có generation + sequence

Mọi log audio quan trọng nên có:

``` text
generation
sequence
pts
frames
sampleRate
channels
ring occupancy
```

Ví dụ:

``` text
[AUDIO]
gen=12
seq=1934
pts=120.384
frames=960
rate=48000
ch=2
raw=3/8
processed=4/16
```

Điều này cực kỳ hữu ích để tìm "audio cũ".

------------------------------------------------------------------------

# 63. Test bắt buộc

## SPSC

``` text
1 producer
1 consumer
10 million blocks
```

Kiểm tra:

``` text
sequence continuity
no duplicate
no overwrite
```

## Seek stress

``` text
seek 0
seek 30
seek 5
seek 100
seek 2
...
```

1000 lần.

Kiểm tra:

``` text
old generation never reaches output
```

## Track change

``` text
Track A
Track B
Track A
Track C
```

## Shutdown stress

``` text
Start
Stop
Start
Stop
...
```

1000 cycles.

## Backend

``` text
SDL
WASAPI
SDL
WASAPI
```

## Pipe disconnect

Kill/restart MPV pipe connection.

## Format

Test:

``` text
44.1k
48k
mono
stereo
5.1
```

Nếu pipeline stereo-only thì 5.1 phải bị normalize/reject có chủ đích,
không được overflow.

------------------------------------------------------------------------

# 64. ThreadSanitizer / sanitizers

MSVC trên Windows không có workflow ThreadSanitizer giống Clang/Linux
thuận tiện như nhau, nhưng vẫn nên:

-   build Debug;
-   enable runtime checks;
-   dùng Application Verifier;
-   dùng AddressSanitizer nếu toolchain hỗ trợ;
-   dùng WinDbg/ETW/WPA;
-   thêm assertions ownership;
-   log native thread IDs.

Đặc biệt kiểm tra:

``` text
AudioProcessor
AudioOutputWorker
AudioCaptureManager
SdlAudioDevice
AudioFilterManager
```

------------------------------------------------------------------------

# 65. P0 --- phải sửa trước

``` text
1. Implement NotifySeekOrTrackChange().
2. Không cho UI consume ProcessedSPSC.
3. Tạo generation request atomic.
4. Gọi discontinuity protocol từ Seek/TrackChange.
5. Sửa visualizer data race.
6. Sửa device ownership.
7. Sửa SDL Write/Close lifetime.
8. Validate channels.
9. Khởi tạo sampleRate/channel_count.
10. Kiểm tra return code của toàn bộ mpv_set_option_string().
11. Di chuyển audio AO options vào pre-mpv_initialize lifecycle.
12. Reset cả raw + processed ring đúng lifecycle.
```

------------------------------------------------------------------------

# 66. P1 --- ổn định pipeline

``` text
1. PCM accumulator.
2. Normalize audio format.
3. AudioClock.
4. Hardware queue state machine.
5. Pause transition handling.
6. Condition variable/event wakeup.
7. Backend command queue.
8. Pipe error classification.
9. Better metrics.
```

------------------------------------------------------------------------

# 67. P2 --- mở rộng STT/subtitle

``` text
1. AudioFanout.
2. AnalysisRing.
3. STT worker.
4. VAD.
5. AudioClock.
6. Subtitle timestamp mapping.
7. Analysis backpressure.
```

------------------------------------------------------------------------

# 68. Kiến trúc mục tiêu

``` text
                             CONTROL PLANE
                                  │
                          AudioCommandQueue
                                  │
              ┌───────────────────┼──────────────────┐
              │                   │                  │
              ▼                   ▼                  ▼
          CaptureCtrl        ProcessorCtrl       OutputCtrl


                              DATA PLANE

                                  MPV
                                   │
                                   │ f32/stereo/48k
                                   ▼
                         ┌──────────────────┐
                         │ CaptureThread    │
                         │ Named Pipe       │
                         │ PCM accumulator  │
                         │ AudioClock       │
                         └────────┬─────────┘
                                  │
                                  ▼
                             RawAudioRing
                                  │
                                  ▼
                         ┌──────────────────┐
                         │ ProcessorThread  │
                         │ RMS / FFT / DSP  │
                         └────────┬─────────┘
                                  │
                     ┌────────────┴────────────┐
                     ▼                         ▼
              PlaybackRing               AnalysisRing
                     │                         │
                     ▼                         ▼
              OutputThread                 STTThread
                     │                         │
                     ▼                         ▼
                 SDL/WASAPI                Subtitle
```

------------------------------------------------------------------------

# 69. Kết luận cuối

Điểm quan trọng nhất:

> **SPSC RingBuffer không phải thủ phạm chính. Ownership quanh
> RingBuffer mới là vấn đề.**

Hiện tại code đã có một nền tảng tốt:

``` text
preallocated AudioBlock
SPSC
atomic metrics
overlapped named pipe
state mutex
worker threads
backend abstraction
```

Nhưng cần loại bỏ các đường truy cập phá ownership:

``` text
UI → SPSC
UI → Device
StopThread → Device lifecycle
```

và chuyển sang:

``` text
UI → CommandQueue
Thread → Own Resource
Ring → exactly one producer + one consumer
```

Nếu sửa đúng P0, pipeline sẽ trở nên ổn định hơn rất nhiều trước khi tối
ưu performance.

------------------------------------------------------------------------

# 70. Checklist triển khai

``` text
[ ] NotifySeekOrTrackChange implemented
[ ] generationRequest atomic
[ ] Seek wired to generation
[ ] TrackChange wired to generation
[ ] UI no longer consumes SPSC
[ ] Shutdown no longer drains SPSC externally
[ ] Visualizer snapshot race removed
[ ] channels validated
[ ] sampleRate initialized
[ ] timepos initialized
[ ] PCM accumulator implemented
[ ] MPV AO options configured before mpv_initialize
[ ] mpv option return codes checked
[ ] obtained SDL format stored
[ ] SDL subsystem ownership fixed
[ ] SdlAudioDevice lifetime owned by OutputThread
[ ] SwitchBackend converted to command
[ ] RawRing reset after workers stop
[ ] ProcessedRing reset after workers stop
[ ] AudioClock implemented
[ ] PTS generation redesigned
[ ] PlaybackRing separated from AnalysisRing
[ ] STT path separated
[ ] metrics expanded
[ ] seek stress test
[ ] shutdown stress test
[ ] pipe reconnect test
[ ] format test
[ ] backend switch test
```

------------------------------------------------------------------------

# 71. Ưu tiên thực tế cho Im_player

Không nên viết lại toàn bộ Audio ngay.

Thứ tự an toàn nhất:

``` text
Phase 1
Ownership + lifetime
        ↓
Phase 2
Seek/generation
        ↓
Phase 3
PCM framing + format
        ↓
Phase 4
Output/device
        ↓
Phase 5
Latency
        ↓
Phase 6
Analysis/STT
```

Đây là con đường ít rủi ro nhất vì vẫn giữ được phần lớn code hiện tại
nhưng loại bỏ các lỗi concurrency nguy hiểm trước.
