# Thiết kế Audio Runtime: Raw PCM và Processed PCM cho Im_player

## 1. Mục tiêu

Mục tiêu của hệ thống audio trong `Im_player`:

- MPV tiếp tục chịu trách nhiệm decode và playback.
- Thu được **RAW PCM** tại điểm trước chuỗi audio filter.
- Thu được **PROCESSED PCM** tại điểm sau chuỗi audio filter.
- RAW và PROCESSED có thể được đọc nội bộ bởi analyzer/AI.
- Có thể xuất RAW và PROCESSED qua hai Windows Named Pipe độc lập.
- Audio realtime không bị block bởi Named Pipe, consumer chậm hoặc analyzer.
- Hỗ trợ nhiều `PlayerSession`.
- Có PTS và sequence để đồng bộ và debug.
- Không phụ thuộc vào `afifo` hoặc `tee=file=...`.

---

# 2. Kết luận từ source hiện tại

Kiến trúc hiện tại đã có nền tảng phù hợp:

```text
PlayerSession
    |
    +-- Player
    +-- PlaybackObserver
    +-- PlaybackCommand
    +-- PlayBackProperty
    +-- PlayerStateSystem
    |
    +-- AudioFilterManager
    +-- VideoFilterManager
```

`AudioFilterManager` hiện đang phù hợp với vai trò **control plane**:

```text
AudioFilterManager
        |
        v
      MPV
        |
        v
   audio filter state
```

Tuy nhiên `f_audio_capture` hiện đang bị đưa vào `AudioFilterManager` để vừa điều khiển filter vừa làm IPC/audio capture. Đây là sai abstraction.

Không nên tiếp tục thiết kế theo:

```text
lavfi
  |
  +-- asplit
  +-- afifo
  +-- tee=file=...
```

Đặc biệt build hiện tại báo:

```text
No such filter: 'afifo'
```

Nhưng vấn đề không chỉ là `afifo` không tồn tại. `Named Pipe`, `audio capture`, `filter management` và `audio processing` là các trách nhiệm khác nhau.

---

# 3. Kiến trúc mục tiêu

Đề xuất tách thành:

```text
PlayerSession
|
+-- PlaybackRuntime
|
+-- RenderRuntime
|
+-- AudioRuntime
|   |
|   +-- AudioFilterManager
|   |
|   +-- AudioCaptureManager
|       |
|       +-- RawAudioStream
|       |   +-- RingBuffer
|       |   +-- PipePublisher
|       |
|       +-- ProcessedAudioStream
|           +-- RingBuffer
|           +-- PipePublisher
|
+-- VideoRuntime
```

Trong đó:

```text
AudioFilterManager
    =
CONTROL PLANE

AudioCaptureManager
    =
DATA PLANE
```

---

# 4. Audio pipeline mục tiêu

```text
                         MPV
                          |
                    decoded audio
                          |
                          v
                   +--------------+
                   |   RAW TAP    |
                   +------+-------+
                          |
                          v
                    Raw PCM stream
                          |
                +---------+---------+
                |                   |
                v                   v
        Internal Ring          Raw Named Pipe
                |                   |
                v                   v
          AI / Analyzer       External Process

                          |
                          v
                    MPV AF Chain
                          |
                          v
                +------------------+
                | PROCESSED TAP    |
                +--------+---------+
                         |
                         v
                 Processed PCM
                         |
                +--------+---------+
                |                  |
                v                  v
        Internal Ring       Processed Named Pipe
                |                  |
                v                  v
          AI / Analyzer      External Process
                         |
                         v
                        AO
```

---

# 5. Nguyên tắc quan trọng

## 5.1 Không dùng Named Pipe làm audio processing mechanism

Named Pipe chỉ nên là transport:

```text
Audio Capture
      |
      +--> Internal Ring Buffer
      |
      +--> Named Pipe Publisher
```

Nếu các component đều ở trong `Im_player.exe`, giao tiếp nội bộ nên dùng ring buffer/queue.

Named Pipe dành cho:

```text
Im_player.exe
      |
      v
External Audio Tool
```

---

## 5.2 Không block audio thread

Không được:

```text
MPV audio thread
      |
      v
WriteFile()
      |
      v
pipe full
      |
      v
BLOCK
```

Phải dùng:

```text
MPV audio thread
      |
      v
SPSC Ring Buffer
      |
      v
PipeWriterThread
      |
      v
WriteFile()
```

Producer audio không phụ thuộc tốc độ consumer.

---

# 6. Hai stream độc lập

## RawAudioStream

```text
RawAudioStream
|
+-- Capture endpoint
+-- AudioRingBuffer
+-- PipePublisher
+-- Statistics
```

## ProcessedAudioStream

```text
ProcessedAudioStream
|
+-- Capture endpoint
+-- AudioRingBuffer
+-- PipePublisher
+-- Statistics
```

Không nối:

```text
Raw -> Processed
```

mà:

```text
                  Audio
                    |
             +------+------+
             |             |
             v             v
          RawTap      ProcessedTap
             |             |
             v             v
          RawStream   ProcessedStream
```

---

# 7. PCM format

Đề xuất format chuẩn cho IPC:

```text
Sample format : S16LE
Sample rate   : 48000 Hz
Channels      : 2
Layout        : Stereo
Interleaved   : Yes
```

Một stereo frame:

```text
L = int16
R = int16
```

Kích thước:

```text
2 bytes/sample
2 channels

=> 4 bytes/frame
```

Bandwidth:

```text
48000 frames/s * 4 bytes
= 192000 bytes/s
= khoảng 192 KB/s
```

Mỗi pipe chỉ khoảng 192 KB/s, rất nhỏ đối với hệ thống hiện đại.

---

# 8. AudioPacket protocol

Không truyền PCM trần:

```text
PCM PCM PCM PCM ...
```

Nên có framing:

```text
HEADER
PCM PAYLOAD

HEADER
PCM PAYLOAD

HEADER
PCM PAYLOAD
```

Đề xuất:

```cpp
#pragma pack(push, 1)

struct AudioPipeHeader
{
    uint32_t magic;
    uint16_t version;
    uint16_t headerSize;

    uint32_t streamType;

    uint64_t sequence;
    int64_t  ptsUs;

    uint32_t sampleRate;
    uint16_t channels;
    uint16_t sampleFormat;

    uint32_t frameCount;
    uint32_t payloadBytes;
};

#pragma pack(pop)
```

Constants:

```cpp
constexpr uint32_t AUDIO_PIPE_MAGIC = 0x4D435041; // "APCM"

enum class AudioStreamType : uint32_t
{
    Raw = 1,
    Processed = 2
};

enum class AudioSampleFormat : uint16_t
{
    S16LE = 1
};
```

---

# 9. Vì sao cần sequence và PTS?

Không thể đồng bộ RAW và PROCESSED chỉ bằng sequence.

AF có thể:

- resample;
- buffer;
- delay;
- thay đổi số frame;
- thêm latency.

Ví dụ:

```text
RAW

sequence = 100
ptsUs    = 1000000
frames   = 1024
```

PROCESSED:

```text
sequence = 73
ptsUs    = 1021333
frames   = 960
```

Sequence dùng để:

- phát hiện drop;
- debug;
- thống kê.

PTS dùng để:

- đồng bộ timeline;
- tính latency;
- ghép RAW/PROCESSED.

---

# 10. AudioFrame nội bộ

Không nên convert S16 thành float ngay tại capture layer.

Đề xuất:

```cpp
struct AudioFrame
{
    int64_t ptsUs;
    uint64_t sequence;

    uint32_t sampleRate;
    uint16_t channels;

    AudioSampleFormat format;

    uint32_t frames;

    std::shared_ptr<AudioBlock> data;
};
```

Nếu analyzer cần float:

```text
S16
 |
 +-- Analyzer A -> float
 +-- Analyzer B -> float
 +-- Recorder   -> giữ S16
```

Capture layer giữ dữ liệu gần với nguồn nhất.

---

# 11. AudioCaptureManager

API đề xuất:

```cpp
class AudioCaptureManager
{
public:
    bool Init(mpv_handle* mpv);
    void Shutdown();

    RawAudioStream& Raw();
    ProcessedAudioStream& Processed();

    void EnableRaw(bool enabled);
    void EnableProcessed(bool enabled);

private:
    RawAudioStream m_raw;
    ProcessedAudioStream m_processed;
};
```

`PlayerSession`:

```cpp
class PlayerSession
{
private:
    std::unique_ptr<AudioFilterManager> m_audioFilterManager;
    std::unique_ptr<AudioCaptureManager> m_audioCapture;
};
```

---

# 12. AudioFilterManager

`AudioFilterManager` chỉ quản lý:

```text
volume
equalizer
compressor
limiter
pan
reverb
resample
...
```

và synchronization:

```text
Application state
       |
       v
AudioFilterManager
       |
       v
build "af" property
       |
       v
mpv_set_property_string()
```

Không nên quản lý:

```text
Named Pipe
Ring Buffer
Audio Capture
Pipe Writer
```

---

# 13. Loại bỏ f_audio_capture khỏi AudioFilterManager

Không tiếp tục:

```cpp
FindFilter("f_audio_capture")
```

và:

```text
@f_audio_capture:lavfi=[...]
```

Thay bằng:

```cpp
m_audioCapture->EnableRaw(true);
m_audioCapture->EnableProcessed(true);
```

Điều này làm API đúng abstraction.

---

# 14. Raw và Processed capture point

Nếu yêu cầu chính xác là:

```text
RAW
  |
  v
AF
  |
  v
PROCESSED
```

thì cần hai capture point trong audio pipeline của mpv.

## Raw

```text
decoder
   |
   v
RAW TAP
   |
   v
AF
```

## Processed

```text
AF
 |
 v
PROCESSED TAP
 |
 v
AO
```

Capture tap phải là dạng passthrough:

```text
input
 |
 +---- copy --> capture
 |
 v
output
```

Nó không được thay đổi frame audio.

---

# 15. Custom MPV audio capture filter

Nếu có thể patch/build mpv, nên thêm native filter:

```text
capture
```

Concept:

```text
@raw_capture:capture=stream=raw
```

và:

```text
@processed_capture:capture=stream=processed
```

Raw:

```text
decoder
   |
   v
raw_capture
   |
   v
AF chain
```

Processed:

```text
AF chain
   |
   v
processed_capture
   |
   v
AO
```

Đây là giải pháp chính xác nhất cho mục tiêu:

```text
raw PCM trước AF
processed PCM sau AF
```

---

# 16. Không dùng afifo

Không nên:

```text
afifo
```

vì build hiện tại không có filter này.

Hơn nữa FIFO trong filter graph không phải IPC mechanism.

Không nên cố thay `afifo` bằng một filter khác chỉ để duy trì kiến trúc cũ.

---

# 17. Không dùng tee=file=... cho IPC

Không nên:

```text
tee=f=s16le:file="..."
```

để làm Named Pipe transport.

Lý do:

- coupling audio filter graph với IPC;
- khó kiểm soát blocking;
- khó framing;
- khó reconnect;
- khó quản lý nhiều session;
- khó đồng bộ PTS;
- khó xử lý consumer chậm.

Capture filter nên đưa dữ liệu vào `AudioCaptureManager`, sau đó manager mới quyết định xuất ra pipe.

---

# 18. Windows Named Pipe

Tên pipe:

```text
\\.\pipe\ImPlayer_<SessionId>_Audio_Raw
\\.\pipe\ImPlayer_<SessionId>_Audio_Processed
```

Không dùng chỉ PID.

Không nên:

```text
mpv_audio_raw_<PID>
```

vì nhiều `PlayerSession` trong cùng process có cùng PID.

Ví dụ:

```text
\\.\pipe\ImPlayer_session_01_Audio_Raw
\\.\pipe\ImPlayer_session_01_Audio_Processed

\\.\pipe\ImPlayer_session_02_Audio_Raw
\\.\pipe\ImPlayer_session_02_Audio_Processed
```

---

# 19. AudioNamedPipe

Đề xuất:

```cpp
class AudioNamedPipe
{
public:
    AudioNamedPipe(
        std::wstring name,
        AudioStreamType type);

    ~AudioNamedPipe();

    bool Start();
    bool WaitForClient();

    bool Write(
        const AudioPipeHeader& header,
        const void* payload,
        uint32_t bytes);

    void Disconnect();
    void Stop();

    bool IsConnected() const;

private:
    std::wstring m_name;
    AudioStreamType m_type;

    HANDLE m_pipe = INVALID_HANDLE_VALUE;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_connected{false};
};
```

---

# 20. Pipe configuration

Đề xuất:

```cpp
CreateNamedPipeW(
    name.c_str(),

    PIPE_ACCESS_OUTBOUND |
    FILE_FLAG_OVERLAPPED,

    PIPE_TYPE_BYTE |
    PIPE_READMODE_BYTE |
    PIPE_WAIT,

    1,

    1024 * 1024,
    1024 * 1024,

    0,

    nullptr);
```

Dùng byte-mode pipe nhưng tự xây framing bằng `AudioPipeHeader`.

Không dựa vào ranh giới `WriteFile`.

---

# 21. Reader phải dùng readExact

Không được giả định:

```cpp
ReadFile(..., sizeof(header), ...)
```

luôn trả đủ header.

Dùng:

```cpp
bool ReadExact(
    HANDLE pipe,
    void* buffer,
    size_t size)
{
    size_t offset = 0;

    while (offset < size)
    {
        DWORD n = 0;

        BOOL ok = ReadFile(
            pipe,
            static_cast<char*>(buffer) + offset,
            static_cast<DWORD>(size - offset),
            &n,
            nullptr);

        if (!ok || n == 0)
            return false;

        offset += n;
    }

    return true;
}
```

Sau đó:

```text
read header
    |
    v
validate
    |
    v
read payloadBytes
    |
    v
push AudioPacket
```

---

# 22. Validate packet

```cpp
bool ValidateHeader(
    const AudioPipeHeader& h)
{
    if (h.magic != AUDIO_PIPE_MAGIC)
        return false;

    if (h.version != 1)
        return false;

    if (h.sampleRate != 48000)
        return false;

    if (h.channels != 2)
        return false;

    if (h.sampleFormat !=
        static_cast<uint16_t>(
            AudioSampleFormat::S16LE))
        return false;

    if (h.frameCount == 0)
        return false;

    uint64_t expected =
        uint64_t(h.frameCount) * 4;

    if (h.payloadBytes != expected)
        return false;

    if (h.payloadBytes > 1024 * 1024)
        return false;

    return true;
}
```

---

# 23. Ring buffer

Không nên dùng:

```cpp
ThreadSafeRingBuffer<float>
```

cho capture mới.

Đề xuất:

```cpp
SpscAudioRingBuffer
```

với topology:

```text
1 producer
    |
    v
1 consumer
```

Nếu cần nhiều analyzer:

```text
AudioFanout
   |
   +-- Analyzer A
   +-- Analyzer B
   +-- Recorder
```

Không biến ring buffer thành MPMC nếu chưa cần.

---

# 24. Overflow policy

Audio producer realtime không được block.

Đề xuất:

```cpp
enum class AudioOverflowPolicy
{
    DropOldest,
    DropNewest,
    Block
};
```

Realtime analyzer:

```text
DropOldest
```

Recorder:

```text
Không block MPV.
Dùng buffering riêng.
```

Không:

```text
MPV audio thread
    |
    v
wait until consumer catches up
```

---

# 25. Pipe writer

```text
Capture
   |
   v
SPSC Queue
   |
   v
AudioPipeWriterThread
   |
   v
Named Pipe
```

API:

```cpp
class AudioPipeWriter
{
public:
    void Start();
    void Stop();

    bool Push(
        AudioPacket packet);

private:
    SpscAudioRingBuffer m_queue;
    AudioNamedPipe m_pipe;
    std::jthread m_worker;
};
```

---

# 26. Consumer không kết nối

MPV vẫn phải chạy bình thường.

Nếu không có client:

```text
No client
   |
   v
drop external packet
```

Không:

```text
No client
   |
   v
block MPV
```

Khi client kết nối:

```text
connect
   |
   v
start publishing
```

---

# 27. Multi-session

Pipe phải gắn với session:

```text
PlayerSession A
 |
 +-- Raw Pipe
 |   \\.\pipe\ImPlayer_A_Audio_Raw
 |
 +-- Processed Pipe
     \\.\pipe\ImPlayer_A_Audio_Processed
```

và:

```text
PlayerSession B
 |
 +-- Raw Pipe
 |   \\.\pipe\ImPlayer_B_Audio_Raw
 |
 +-- Processed Pipe
     \\.\pipe\ImPlayer_B_Audio_Processed
```

Không dùng global audio pipe.

---

# 28. Lifetime

`AudioCaptureManager` phải sống cùng `PlayerSession`.

Startup:

```text
PlayerSession::Init
       |
       v
AudioCaptureManager::Init
       |
       v
create streams
       |
       v
start workers
       |
       v
initialize MPV
```

Shutdown:

```text
PlayerSession::Shutdown
       |
       v
Stop AudioCapture
       |
       v
stop workers
       |
       v
disconnect pipes
       |
       v
shutdown MPV
```

Không được:

```text
destroy MPV
    |
    v
capture thread vẫn chạy
```

---

# 29. Lifetime và raw pointer

Project hiện có các API kiểu:

```cpp
PlayerSession* GetSession(...);
Player* GetPlayer(...);
```

và session có thể bị erase trong manager.

Điều này nguy hiểm khi thread audio giữ raw pointer.

Không để worker giữ lâu dài:

```cpp
PlayerSession*
```

Worker chỉ nên giữ context/ownership cần thiết của capture subsystem.

---

# 30. `SafeExecute` không phải lifetime safety

Pattern:

```cpp
if (this)
{
    func(*this);
}
```

không bảo vệ object đã bị destroy.

Nếu `this` là dangling pointer, `if (this)` vẫn có thể true.

Với audio worker, lifetime phải được đảm bảo bằng:

- ownership;
- stop token;
- join;
- explicit shutdown ordering;
- không giữ raw pointer vượt lifetime.

---

# 31. Shutdown ordering

Đề xuất:

```text
PlayerSession::Shutdown
        |
        v
Stop AudioCapture
        |
        v
Stop Audio workers
        |
        v
Disconnect pipes
        |
        v
Stop Observer
        |
        v
Stop Renderer
        |
        v
Stop Player / MPV
```

Audio capture phải dừng trước MPV.

---

# 32. `AudioCaptureEngine` hiện tại cần refactor

Không nên giữ nguyên:

```text
AudioCaptureEngine
```

với trách nhiệm:

- create pipe;
- connect pipe;
- read PCM;
- convert float;
- ring buffer;
- worker;
- capture state.

Thay bằng:

```text
AudioCaptureManager
|
+-- RawAudioStream
|   +-- RawCaptureEndpoint
|   +-- RawRingBuffer
|   +-- RawPipePublisher
|
+-- ProcessedAudioStream
    +-- ProcessedCaptureEndpoint
    +-- ProcessedRingBuffer
    +-- ProcessedPipePublisher
```

---

# 33. File structure đề xuất

```text
audio/
|
+-- AudioRuntime.h
+-- AudioRuntime.cpp
|
+-- AudioPacket.h
+-- AudioPacket.cpp
|
+-- AudioRingBuffer.h
+-- AudioRingBuffer.cpp
|
+-- AudioNamedPipe.h
+-- AudioNamedPipe.cpp
|
+-- AudioPipeWriter.h
+-- AudioPipeWriter.cpp
|
+-- AudioPipeReader.h
+-- AudioPipeReader.cpp
|
+-- AudioStream.h
+-- AudioStream.cpp
|
+-- RawAudioStream.h
+-- RawAudioStream.cpp
|
+-- ProcessedAudioStream.h
+-- ProcessedAudioStream.cpp
|
+-- AudioCaptureManager.h
+-- AudioCaptureManager.cpp
|
+-- AudioFilterManager.h
+-- AudioFilterManager.cpp
```

MPV-specific:

```text
mpv/
|
+-- af_capture.c
+-- af_capture.h
```

hoặc module tương đương tùy cấu trúc source mpv mà bạn fork/build.

---

# 34. Phase triển khai

Không nên sửa mọi thứ cùng lúc.

## Phase 1 — Protocol

Implement:

```text
AudioPipeHeader
AudioPacket
AudioStreamType
AudioSampleFormat
```

## Phase 2 — Buffer

Implement:

```text
SpscAudioRingBuffer
```

## Phase 3 — IPC

Implement:

```text
AudioNamedPipe
AudioPipeWriter
AudioPipeReader
```

## Phase 4 — Streams

Implement:

```text
RawAudioStream
ProcessedAudioStream
```

## Phase 5 — Runtime

Implement:

```text
AudioCaptureManager
AudioRuntime
```

và tích hợp vào `PlayerSession`.

## Phase 6 — MPV raw tap

Thêm capture filter cho RAW.

## Phase 7 — MPV processed tap

Thêm capture filter cho PROCESSED.

## Phase 8 — PTS

Thêm:

```text
sequence
ptsUs
latency
drop statistics
```

## Phase 9 — Analyzer/AI

Kết nối:

```text
RawAudioStream
ProcessedAudioStream
```

vào các analyzer.

---

# 35. Kiến trúc cuối cùng

```text
                         PlayerSession
                              |
                         AudioRuntime
                              |
             +----------------+----------------+
             |                                 |
             v                                 v
     AudioFilterManager                 AudioCaptureManager
             |                                 |
             |                                 |
             v                                 |
            MPV                                |
             |                                 |
       decoded audio                          |
             |                                 |
             v                                 |
          RAW TAP -----------------------------+
             |                                 |
             v                                 |
        RawAudioStream                         |
             |                                 |
       +-----+------+                          |
       |            |                          |
       v            v                          |
   RingBuffer    PipeWriter                    |
       |            |                          |
       v            v                          |
      AI        Raw Named Pipe                 |
                                                |
             MPV AF Chain                      |
                  |                             |
                  v                             |
            PROCESSED TAP ----------------------+
                  |
                  v
       ProcessedAudioStream
                  |
          +-------+-------+
          |               |
          v               v
      RingBuffer     PipeWriter
          |               |
          v               v
         AI        Processed Named Pipe
```

---

# 36. Quyết định kiến trúc

| Thành phần | Quyết định |
|---|---|
| `PlayerSession` | Giữ |
| `AudioFilterManager` | Giữ, chỉ control plane |
| `AudioCaptureManager` | Thêm |
| `RawAudioStream` | Thêm |
| `ProcessedAudioStream` | Thêm |
| `AudioPacket` | Thêm |
| `SPSC AudioRingBuffer` | Thêm |
| `AudioNamedPipe` | Thêm/refactor |
| `AudioPipeWriter` | Thêm |
| `AudioPipeReader` | Thêm |
| `f_audio_capture` trong FilterManager | Bỏ |
| `afifo` | Bỏ |
| `tee=file=...` | Bỏ |
| `ThreadSafeRingBuffer<float>` | Thay |
| Convert S16 -> float tại capture | Bỏ |
| Blocking push trong realtime path | Bỏ |
| PID-only pipe name | Bỏ |
| Session-based pipe name | Dùng |
| Sequence | Dùng |
| PTS | Bắt buộc |
| Dedicated writer thread | Bắt buộc |
| Custom MPV raw tap | Nên có |
| Custom MPV processed tap | Nên có |

---

# 37. Kết luận

Không nên tiếp tục sửa lỗi:

```text
No such filter: afifo
```

theo hướng tìm một filter thay thế.

Cần sửa kiến trúc:

```text
OLD

AudioFilterManager
    |
    +-- f_audio_capture
    |     |
    |     +-- lavfi
    |     +-- afifo
    |     +-- tee
    |     +-- pipe
    |
    +-- normal filters
```

thành:

```text
NEW

PlayerSession
    |
    +-- AudioFilterManager
    |       |
    |       +-- normal MPV AF
    |
    +-- AudioCaptureManager
            |
            +-- RawAudioStream
            |     |
            |     +-- RingBuffer
            |     +-- Raw Pipe
            |
            +-- ProcessedAudioStream
                  |
                  +-- RingBuffer
                  +-- Processed Pipe
```

Đây là hướng phù hợp nhất với mục tiêu hiện tại của `Im_player`, đồng thời mở đường cho:

```text
Audio
 |
 +-- FFT
 +-- VAD
 +-- Speech Recognition
 +-- TTS synchronization
 +-- Waveform
 +-- AI processing
 +-- External Python/AI process
```

mà không phải phá lại `PlayerSession` sau này.

---

# 38. Bước triển khai tiếp theo

Thứ tự nên là:

```text
AudioPacket
    ↓
SPSC AudioRingBuffer
    ↓
AudioNamedPipe
    ↓
AudioPipeWriter
    ↓
AudioPipeReader
    ↓
RawAudioStream
    ↓
ProcessedAudioStream
    ↓
AudioCaptureManager
    ↓
PlayerSession integration
    ↓
MPV raw capture filter
    ↓
MPV processed capture filter
    ↓
PTS synchronization
```

# Checklist
- Đã loại bỏ `f_audio_capture`,`afifo`,`tee=file=...` ra khỏi AudioFilterManager.
- Đã tạo bộ khung cho `AudioCaptureManager`, nhưng vẫn triển khai gì cả.
- Triển khai `AudioCaptureManager` cơ bản và đăng kí khởi tạo trong `PlayerSession` .

Đây là thứ tự có rủi ro thấp nhất và cho phép test từng tầng độc lập.
