# Phân tích và thiết kế lại AudioCaptureManager – Raw PCM / Processed PCM cho libmpv trên Windows

## 1. Mục tiêu

Thiết kế lại hệ thống capture audio hiện tại của `Im_player` nhằm:

- Lấy được PCM audio từ `libmpv`.
- Giảm CPU overhead và latency.
- Không để `SDL_Audio` làm nghẽn audio capture.
- Không lock mutex cho từng sample.
- Tách rõ:
  - Raw PCM.
  - Processed PCM.
  - Audio Output.
  - Audio Analysis.
  - Subtitle/STT.
- Có khả năng mở rộng về sau cho:
  - Speech-to-Text.
  - Subtitle realtime.
  - Visualizer.
  - FFT.
  - Audio filter.
  - Translation.
  - Recording.
- Hoạt động ổn định trên Windows.
- Có thể mở rộng thành hệ thống audio pipeline độc lập với `SDL`.

---

# 2. Kiến trúc hiện tại

Kiến trúc hiện tại về cơ bản là:

```text
                         MPV
                          |
                          v
                   ao=pcm / pipe
                          |
                          v
                  Named Pipe Server
                          |
                          v
                  CaptureLoop Thread
                          |
             +------------+------------+
             |                         |
             v                         v
       SDL_QueueAudio()       ThreadSafeRingBuffer<float>
             |                         |
             v                         v
          Speaker               STT / Visualizer
```

Trong code hiện tại:

```cpp
mpv_set_option_string(m_mpv, "ao", "pcm");
mpv_set_option_string(m_mpv, "ao-pcm-file", m_pipeName.c_str());
mpv_set_option_string(m_mpv, "audio-samplerate", "48000");
mpv_set_option_string(m_mpv, "audio-channels", "stereo");
mpv_set_option_string(m_mpv, "audio-format", "f32le");
```

Sau đó `CaptureLoop()` đọc pipe:

```cpp
ReadFile(
    currentPipe,
    pcmFloatBuffer.data(),
    static_cast<DWORD>(
        pcmFloatBuffer.size() * sizeof(float)
    ),
    &bytesRead,
    &overlapped
);
```

rồi thực hiện đồng thời:

```text
ReadFile
   |
   +--> SDL_QueueAudio
   |
   +--> RingBuffer
```

Đây chính là điểm cần thay đổi.

---

# 3. Vấn đề hiệu năng nghiêm trọng nhất

## 3.1. Push từng float

Code hiện tại:

```cpp
for (int i = 0; i < samplesRead; ++i) {
    float sample = pcmFloatBuffer[i];

    if (!m_preFilterBuffer.try_push(sample)) {
        float dummy;
        m_preFilterBuffer.try_pop(dummy);
        m_preFilterBuffer.try_push(sample);
    }
}
```

`m_preFilterBuffer` là:

```cpp
ThreadSafeRingBuffer<float>
```

và mỗi `try_push()` thực hiện:

```cpp
std::unique_lock<std::mutex> lock(m_mutex);
```

Như vậy mỗi sample đều có mutex operation.

---

# 4. Tính toán overhead

Audio:

```text
Sample rate = 48,000 Hz
Channels    = 2
```

Số float mỗi giây:

```text
48,000 × 2 = 96,000 float/s
```

Mỗi float là:

```text
4 bytes
```

Bandwidth:

```text
96,000 × 4
= 384,000 bytes/s
≈ 375 KiB/s
```

Bandwidth này rất nhỏ đối với máy tính hiện đại.

Vấn đề không nằm ở bandwidth.

Vấn đề nằm ở:

```text
96,000 mutex operations / second
```

và các thao tác:

```text
lock
unlock
condition/state checking
ring index update
branch
try_push
try_pop
```

Điều này không cần thiết.

---

# 5. Sai abstraction: RingBuffer<float>

Audio streaming không nên được quản lý ở mức:

```cpp
RingBuffer<float>
```

mà nên ở mức:

```cpp
RingBuffer<AudioBlock>
```

Ví dụ:

```cpp
struct AudioBlock
{
    uint64_t pts;

    uint32_t sampleRate;
    uint16_t channels;
    uint16_t format;

    uint32_t frames;

    std::array<float, 4096 * 2> samples;
};
```

Nếu:

```text
sampleRate = 48000
channels   = 2
frames     = 2048
```

thì:

```text
2048 × 2 = 4096 float
```

Mỗi block:

```text
4096 × 4
= 16 KB
```

Số block/giây:

```text
48000 / 2048
≈ 23.44 block/s
```

Thay vì:

```text
96,000 operations/s
```

chúng ta chỉ cần khoảng:

```text
23 operations/s
```

ở mức block.

Đây là thay đổi quan trọng nhất.

---

# 6. Kiến trúc mới

Kiến trúc đề xuất:

```text
                             MPV
                              |
                              v
                    +-------------------+
                    | MPV Audio Output  |
                    +---------+---------+
                              |
                              v
                    +-------------------+
                    | Named Pipe        |
                    | Raw PCM Transport |
                    +---------+---------+
                              |
                              v
                    +-------------------+
                    | AudioCaptureThread|
                    |                   |
                    | ONLY I/O          |
                    +---------+---------+
                              |
                              v
                    +-------------------+
                    | Raw Audio Stream  |
                    +---------+---------+
                              |
                              v
                    +-------------------+
                    | Audio Dispatcher  |
                    +---------+---------+
                              |
             +----------------+----------------+
             |                |                |
             v                v                v
       Output Ring       Analysis Ring     Monitor Ring
             |                |
             v                v
       Audio Output      Processing
             |                |
             v                v
            SDL       Processed PCM
                              |
                    +---------+---------+
                    |                   |
                    v                   v
                  STT              Visualizer
```

---

# 7. Nguyên tắc quan trọng

## 7.1. Capture thread chỉ capture

`AudioCaptureThread` không nên:

- chạy filter.
- chạy FFT.
- chạy STT.
- render.
- gọi logic subtitle.
- xử lý visualization.
- thực hiện các operation nặng.

Nó chỉ làm:

```text
Read pipe
   ↓
Parse packet
   ↓
Create AudioBlock
   ↓
Push Raw PCM
```

Mục tiêu:

```text
Capture thread = fast path
```

---

# 8. Tách Audio Output khỏi Capture

Hiện tại:

```text
CaptureLoop
   |
   +--> SDL_QueueAudio
```

nên đổi thành:

```text
Capture
   |
   v
Raw Stream
   |
   v
AudioOutputWorker
   |
   v
SDL
```

Capture không cần quan tâm SDL đang:

- đầy buffer.
- thiếu buffer.
- bị pause.
- bị device issue.
- bị latency.

---

# 9. AudioBlock

Đề xuất:

```cpp
struct AudioBlock
{
    uint64_t sequence = 0;

    double pts = 0.0;

    uint32_t sampleRate = 48000;

    uint16_t channels = 2;

    AudioSampleFormat format =
        AudioSampleFormat::Float32;

    uint32_t frames = 0;

    std::array<float, 4096 * 2> samples{};
};
```

Có thể dùng:

```cpp
enum class AudioSampleFormat
{
    Float32,
    Int16
};
```

Trong phiên bản đầu tiên nên cố định:

```text
Float32
48 kHz
Stereo
```

để giảm complexity.

---

# 10. Vì sao cần PTS?

Audio không chỉ là:

```text
float float float float...
```

mà phải gắn với timeline:

```text
AudioBlock
   |
   +-- PTS
   +-- duration
   +-- sample rate
   +-- frames
```

Ví dụ:

```text
Block 0
PTS = 100.000
Frames = 2048
Duration = 2048 / 48000
         ≈ 42.67 ms
```

Block tiếp theo:

```text
PTS = 100.04267
```

Điều này cực kỳ quan trọng cho subtitle.

Sau này:

```text
Audio PTS
    |
    v
STT timestamp
    |
    v
Subtitle timeline
```

Nếu không có timestamp, realtime subtitle sẽ khó đồng bộ chính xác với video.

---

# 11. Raw PCM Stream

Raw stream là audio sau khi lấy ra từ MPV nhưng chưa filter.

```text
MPV
 |
 v
Raw PCM
 |
 +--> SDL
 |
 +--> STT
 |
 +--> Recorder
 |
 +--> Analyzer
 |
 +--> Filter
```

Raw PCM phải được xem là nguồn dữ liệu chuẩn.

---

# 12. Processed PCM Stream

Processed PCM là output của audio processing:

```text
Raw PCM
   |
   v
Audio Processor
   |
   +--> volume
   +--> normalize
   +--> EQ
   +--> noise reduction
   +--> resample
   +--> channel processing
   |
   v
Processed PCM
```

Sau đó:

```text
Processed PCM
       |
       +--> SDL
       +--> Visualizer
       +--> Recording
       +--> Analysis
```

---

# 13. Không dùng một RingBuffer cho nhiều consumer

Một vấn đề rất quan trọng:

```text
RawRing
  |
  +--> SDL
  |
  +--> STT
  |
  +--> Visualizer
```

Không thể dùng một SPSC ring như vậy.

Nếu có:

```text
Producer = Capture
Consumers = SDL + STT + Visualizer
```

thì phải có broadcast/dispatcher.

Kiến trúc:

```text
                     Capture
                        |
                        v
                  AudioDispatcher
                        |
           +------------+------------+
           |            |            |
           v            v            v
        SDL Ring     STT Ring     Visualizer Ring
```

Mỗi consumer có queue riêng.

---

# 14. SPSC RingBuffer

Trong trường hợp:

```text
Producer
   |
   v
Ring
   |
   v
Consumer
```

thì dùng:

```text
SPSC = Single Producer Single Consumer
```

Không cần mutex.

Thiết kế:

```cpp
template<typename T>
class SpscRingBuffer
{
public:
    explicit SpscRingBuffer(size_t capacity);

    bool try_push(T&& value);

    bool try_pop(T& value);

    size_t size() const noexcept;

    size_t capacity() const noexcept;

private:
    std::vector<T> m_buffer;

    size_t m_capacity;

    std::atomic<size_t> m_head{0};

    std::atomic<size_t> m_tail{0};
};
```

---

# 15. Vì sao SPSC phù hợp

Audio pipeline thường có mô hình:

```text
AudioCaptureThread
        |
        v
RawRing
        |
        v
AudioOutputThread
```

Chính xác là:

```text
1 producer
1 consumer
```

SPSC rất phù hợp.

Không cần:

```cpp
std::mutex
```

trên fast path.

---

# 16. Drop policy

Audio realtime không nên block vô thời hạn.

Không nên:

```cpp
push()
```

chờ buffer trống.

Nếu consumer chậm:

```text
Capture
   |
   v
Ring full
```

Capture không nên đứng chờ lâu.

Có thể dùng:

```text
DROP_OLDEST
```

Ví dụ:

```text
Ring:

A B C D E F
^

consumer chậm

push G

=> B C D E F G
```

Điều này phù hợp cho:

- visualizer.
- realtime STT.
- live analysis.

Nhưng đối với audio output cần cân nhắc kỹ vì drop audio sẽ gây click/gap.

Do đó mỗi stream có thể có policy riêng.

---

# 17. Drop policy đề xuất

## Audio Output

```text
DROP = NO
```

Nếu underrun:

```text
insert silence
```

Nếu overflow:

```text
giảm queue / resync
```

## STT

```text
DROP_OLDEST
```

vì STT realtime không cần xử lý audio quá cũ.

## Visualizer

```text
DROP_OLDEST
```

vì frame cũ không còn giá trị.

## Recorder

```text
DROP = NO
```

vì recording phải chính xác.

---

# 18. SDL Audio

SDL chỉ nên là consumer:

```text
Processed/Raw PCM
       |
       v
AudioOutputWorker
       |
       v
SDL_QueueAudio
```

Không để:

```text
CaptureThread
       |
       v
SDL
```

---

# 19. SDL queue latency

Code hiện tại:

```cpp
SDL_GetQueuedAudioSize(m_audioDevice)
```

với khoảng:

```text
0.5 second
```

có thể tạo latency.

Ví dụ:

```text
MPV audio
   |
   | 500 ms
   v
SDL queue
   |
   v
Speaker
```

Đối với player realtime và subtitle:

```text
500 ms
```

là khá lớn.

Nên bắt đầu thử:

```text
50–150 ms
```

và benchmark thực tế.

---

# 20. Named Pipe

Pipe hiện tại:

```cpp
PIPE_TYPE_BYTE
```

có nghĩa là byte stream.

Không nên giả định một `ReadFile()` tương ứng với một audio packet.

Ví dụ:

```text
Write:
[Header][Payload]

Read:
[Header một phần]

Read:
[Payload một phần]

Read:
[Payload phần còn lại]
```

Do đó phải có parser/framer.

---

# 21. Audio Packet

Nên có protocol nội bộ:

```cpp
struct AudioPacketHeader
{
    uint32_t magic;

    uint16_t version;

    uint16_t headerSize;

    uint64_t sequence;

    int64_t ptsUs;

    uint32_t sampleRate;

    uint16_t channels;

    uint16_t format;

    uint32_t frames;

    uint32_t payloadBytes;
};
```

Ví dụ:

```text
MAGIC
VERSION
SEQUENCE
PTS
SAMPLE RATE
CHANNELS
FORMAT
FRAMES
PAYLOAD SIZE
PAYLOAD
```

---

# 22. Vì sao cần Sequence

Ví dụ:

```text
Block 100
Block 101
Block 102
Block 103
```

Nếu xảy ra:

```text
100
101
103
```

có thể phát hiện:

```text
missing block 102
```

Điều này rất hữu ích để debug audio pipeline.

---

# 23. Vì sao cần PTS

Sequence dùng để kiểm tra:

```text
packet continuity
```

PTS dùng để kiểm tra:

```text
timeline continuity
```

Hai thứ khác nhau.

---

# 24. Audio duration

Với:

```text
frames = 2048
sampleRate = 48000
```

duration:

```text
duration = frames / sampleRate

         = 2048 / 48000

         ≈ 0.0426667 sec
```

Mỗi block khoảng:

```text
42.67 ms
```

Đây là mức hợp lý cho pipeline đầu tiên.

---

# 25. Có nên dùng 1024 hay 2048 frames?

## 1024

```text
1024 / 48000
≈ 21.33 ms
```

Ưu điểm:

- latency thấp.
- realtime tốt.

Nhược điểm:

- nhiều packet hơn.

## 2048

```text
2048 / 48000
≈ 42.67 ms
```

Ưu điểm:

- ít overhead.
- CPU tốt.
- dễ xử lý.

Nhược điểm:

- latency cao hơn.

## Khuyến nghị

Phiên bản đầu:

```text
2048 frames
```

Sau khi hệ thống ổn định:

```text
1024 frames
```

nếu cần giảm latency.

---

# 26. Kiến trúc class đề xuất

```text
AudioCaptureManager
│
├── MPVAudioSource
│
├── NamedPipeServer
│
├── AudioCaptureThread
│
├── AudioDispatcher
│
├── AudioOutput
│   └── SDLAudioOutput
│
├── AudioProcessor
│
├── RawAudioStream
│
├── ProcessedAudioStream
│
└── AudioAnalysis
```

---

# 27. Trách nhiệm AudioCaptureManager

`AudioCaptureManager` chỉ orchestration:

```text
Init
Start
Stop
Shutdown
```

Không nên chứa toàn bộ logic.

Ví dụ:

```cpp
class AudioCaptureManager
{
public:

    bool init(
        mpv_handle* mpv,
        PlayerStateSystem* state
    );

    bool start();

    void stop();

    void shutdown();

private:

    std::unique_ptr<MPVAudioSource> m_source;

    std::unique_ptr<AudioDispatcher> m_dispatcher;

    std::unique_ptr<AudioOutput> m_output;

    std::unique_ptr<AudioProcessor> m_processor;
};
```

---

# 28. MPVAudioSource

Chịu trách nhiệm:

```text
MPV
 ↓
PCM transport
```

Không biết:

- SDL.
- STT.
- Visualizer.

---

# 29. NamedPipeAudioTransport

Chịu trách nhiệm:

```text
CreateNamedPipe
ConnectNamedPipe
ReadFile
DisconnectNamedPipe
CloseHandle
```

Tách Windows-specific code ra khỏi audio logic.

---

# 30. AudioCaptureThread

Chịu trách nhiệm:

```text
pipe
 ↓
packet
 ↓
AudioBlock
```

Không xử lý audio nặng.

---

# 31. AudioDispatcher

Chịu trách nhiệm broadcast:

```text
AudioBlock
   |
   +--> Output
   +--> Processor
   +--> STT
   +--> Visualizer
```

Có thể thiết kế:

```cpp
class AudioDispatcher
{
public:

    void publish(const AudioBlock& block);

private:

    AudioStream m_outputStream;

    AudioStream m_analysisStream;

    AudioStream m_visualizerStream;
};
```

---

# 32. AudioProcessor

```text
Raw PCM
   |
   v
AudioProcessor
   |
   v
Processed PCM
```

Ví dụ:

```cpp
class AudioProcessor
{
public:

    AudioBlock process(
        const AudioBlock& input
    );
};
```

Sau này có thể thay bằng chain:

```text
AudioProcessorChain
       |
       +--> Gain
       +--> EQ
       +--> NoiseReduction
       +--> Resampler
```

---

# 33. Raw PCM và Processed PCM phải độc lập

Nên có:

```text
RawAudioStream
```

và:

```text
ProcessedAudioStream
```

Không nên:

```text
m_preFilterBuffer
m_postFilterBuffer
```

chỉ đơn giản là hai `vector<float>`.

Thay vào đó:

```text
RawAudioStream
    |
    +--> AudioBlock

ProcessedAudioStream
    |
    +--> AudioBlock
```

---

# 34. Buffer size

Không nên:

```cpp
m_preFilterBuffer(96000)
```

vì:

```text
96000 float
```

không thể hiện rõ:

- bao nhiêu frame.
- bao nhiêu channel.
- bao nhiêu milliseconds.
- timestamp nào.

Nên tính theo audio duration.

Ví dụ:

```text
Raw buffer = 500 ms

48000 × 0.5
= 24000 frames

stereo:
24000 × 2
= 48000 float
```

Nhưng ring chứa `AudioBlock`, không chứa float.

---

# 35. Ví dụ capacity

```text
Block = 2048 frames
Buffer = 500 ms

500 / 42.67
≈ 11.7 block
```

Có thể chọn:

```text
16 blocks
```

hoặc:

```text
32 blocks
```

Ví dụ:

```cpp
SpscRingBuffer<AudioBlock> ring(32);
```

---

# 36. Audio latency budget

Nên suy nghĩ theo:

```text
MPV
 ↓
Pipe
 ↓
Capture
 ↓
Ring
 ↓
Processor
 ↓
Output Ring
 ↓
SDL
 ↓
Device
```

Latency tổng:

```text
Ltotal =
    Lmpv
  + Lpipe
  + Lcapture
  + Lprocessing
  + Lqueue
  + Ldevice
```

Nếu SDL queue = 500 ms:

```text
Lqueue ≈ 500 ms
```

thì dù các phần còn lại rất nhanh, tổng latency vẫn cao.

---

# 37. Không nên dùng `std::cout` quá nhiều trong audio thread

Code hiện tại:

```cpp
LOG(...)
```

nếu logger thực hiện:

```text
mutex
filesystem
console I/O
formatting
```

thì audio thread có thể bị stall.

Nên:

```text
normal path
    = no logging
```

Chỉ log:

```text
connect
disconnect
error
overflow
underrun
```

và rate-limit các log lặp lại.

---

# 38. Không cấp phát memory trong capture loop

Hiện tại:

```cpp
std::vector<float> pcmFloatBuffer(2048);
```

nằm trong vòng xử lý connection nhưng vẫn nên tránh các allocation không cần thiết.

Tốt hơn:

```cpp
AudioBlock block;
```

được tạo một lần.

Hoặc dùng pool:

```text
AudioBlockPool
```

nếu sau này throughput lớn hơn.

---

# 39. AudioBlockPool

Có thể thiết kế:

```text
Pool
 ├── Block 0
 ├── Block 1
 ├── Block 2
 ├── ...
 └── Block 31
```

Producer lấy:

```text
free block
```

Consumer trả:

```text
used block
```

Điều này tránh:

```text
new
delete
malloc
free
```

trong audio pipeline.

---

# 40. Tuy nhiên không cần làm Pool ngay

Giai đoạn đầu:

```text
AudioBlock
+
SPSC Ring
```

là đủ.

Chỉ thêm pool nếu profiling chứng minh allocation là bottleneck.

Không nên over-engineering ngay từ đầu.

---

# 41. Stop/Shutdown

Code hiện tại có một vấn đề:

```cpp
CancelIoEx(...)
DisconnectNamedPipe(...)
CloseHandle(...)
```

được thực hiện từ thread khác trong khi capture thread đang sử dụng handle.

Đây là vùng cần thiết kế cẩn thận.

Không nên để:

```text
Thread A:
CloseHandle(pipe)

Thread B:
ReadFile(pipe)
```

một cách không kiểm soát.

---

# 42. Ownership của pipe

Nên để:

```text
AudioCaptureThread
```

là owner thực sự của handle pipe.

Shutdown chỉ gửi:

```text
stop request
```

và signal event.

Sau đó capture thread tự:

```text
Cancel I/O
Disconnect
CloseHandle
```

rồi kết thúc.

Mô hình:

```text
Main Thread
    |
    | stop()
    v
Stop Event
    |
    v
Capture Thread
    |
    +--> Cancel
    +--> Disconnect
    +--> Close
    +--> Exit
```

Điều này an toàn hơn.

---

# 43. Windows overlapped I/O

Nên có:

```text
OVERLAPPED
    |
    +--> hEvent
```

và một stop event riêng:

```text
hIoEvent
hStopEvent
```

Thay vì:

```cpp
WaitForSingleObject(hEvent, 100);
```

lặp lại.

Có thể sử dụng:

```text
WaitForMultipleObjects
```

để chờ:

```text
I/O complete
OR
Shutdown
```

Điều này làm shutdown nhanh hơn và sạch hơn.

---

# 44. Capture thread nên có trạng thái

Ví dụ:

```cpp
enum class AudioCaptureState
{
    Stopped,
    Starting,
    WaitingForMPV,
    Connected,
    Capturing,
    Stopping,
    Failed
};
```

Điều này giúp debug.

---

# 45. Metrics

Nên thêm:

```cpp
struct AudioPipelineMetrics
{
    uint64_t packetsReceived;
    uint64_t packetsDropped;
    uint64_t bytesReceived;

    uint64_t ringOverflows;
    uint64_t ringUnderflows;

    uint64_t pipeErrors;

    uint64_t lastSequence;

    double lastPTS;
};
```

Có thể hiển thị trong debug UI.

---

# 46. Metrics rất quan trọng

Nếu subtitle bị trễ, cần biết:

```text
Pipe latency?
Ring latency?
STT latency?
SDL latency?
Processing latency?
```

Không có metrics thì rất khó debug.

---

# 47. Đo latency

Mỗi block:

```text
capture timestamp
PTS
processing timestamp
output timestamp
```

Ví dụ:

```text
PTS                = 100.000 s
Capture             = 100.010 s
Processor            = 100.015 s
Output enqueue       = 100.020 s
```

Có thể biết:

```text
capture latency = 10 ms
processing      = 5 ms
output enqueue  = 5 ms
```

---

# 48. Subtitle pipeline

Sau redesign:

```text
                  Raw PCM
                     |
                     v
               STT Worker
                     |
                     v
              Speech Segment
                     |
                     v
              Subtitle Engine
                     |
                     v
                  ImGui UI
```

STT không chạy trên capture thread.

---

# 49. STT không nên đọc trực tiếp Named Pipe

Không nên:

```text
Named Pipe
  ├── Capture
  └── STT
```

vì:

- ownership phức tạp.
- synchronization khó.
- dễ mất packet.
- không có một nguồn dữ liệu chuẩn.

Nên:

```text
Pipe
 |
 v
Capture
 |
 v
Raw Stream
 |
 v
STT
```

---

# 50. Visualizer

Visualizer cũng không nên đọc trực tiếp pipe.

```text
Raw PCM
   |
   v
Visualizer Ring
   |
   v
FFT
   |
   v
UI Snapshot
```

UI thread chỉ đọc snapshot.

Không chạy FFT trong render thread.

---

# 51. Thread model đề xuất

```text
Main/UI Thread
       |
       +--------------------+
                            |
AudioCaptureThread          |
       |                    |
       v                    |
Raw Dispatcher              |
       |                    |
       +--------+-----------+
                |
       +--------+--------+
       |                 |
       v                 v
AudioOutputThread   AudioProcessThread
       |                 |
       v                 v
      SDL          Processed Stream
                         |
                 +-------+-------+
                 |               |
                 v               v
                STT         Visualizer
```

---

# 52. Không cần tạo quá nhiều thread ngay lập tức

Phiên bản đầu có thể:

```text
Thread 1:
Capture

Thread 2:
Audio Output

Thread 3:
Audio Processing
```

STT có thể có worker riêng sau.

Không nên tạo:

```text
1 thread/sample
```

hoặc:

```text
1 thread/consumer
```

một cách không kiểm soát.

---

# 53. Data ownership

Quy tắc nên là:

```text
Capture owns input
       |
       v
AudioBlock
       |
       v
Dispatcher
       |
       +--> copy/move to consumers
```

Nếu performance cần cao hơn:

```text
shared immutable AudioBlock
```

hoặc:

```text
AudioBlockPool
```

nhưng chưa cần ngay.

---

# 54. Không nên dùng shared_ptr ở fast path nếu chưa cần

Không nên mặc định:

```cpp
std::shared_ptr<AudioBlock>
```

cho mỗi packet.

Reference counting cũng có overhead.

Ưu tiên:

```cpp
AudioBlock
```

move/copy theo block.

16 KB/block không lớn.

---

# 55. Tách Raw và Processed

API nên giống:

```cpp
class AudioStream
{
public:

    bool tryRead(AudioBlock& block);

    size_t available() const;

};
```

Sau đó:

```cpp
RawAudioStream
ProcessedAudioStream
```

có thể có implementation riêng.

---

# 56. API đề xuất

```cpp
class AudioCaptureManager
{
public:

    bool Init(
        mpv_handle* mpv,
        PlayerStateSystem* stateSystem
    );

    bool Start();

    void Stop();

    void Shutdown();

    AudioStream& RawStream();

    AudioStream& ProcessedStream();

    AudioPipelineMetrics GetMetrics() const;
};
```

Consumer:

```cpp
AudioBlock block;

while (rawStream.tryRead(block))
{
    stt.Process(block);
}
```

---

# 57. Audio Output API

```cpp
class IAudioOutput
{
public:

    virtual ~IAudioOutput() = default;

    virtual bool open(
        const AudioFormat& format
    ) = 0;

    virtual void write(
        const AudioBlock& block
    ) = 0;

    virtual void flush() = 0;

    virtual void close() = 0;
};
```

Implementation:

```cpp
SDLAudioOutput
```

Sau này có thể:

```text
WASAPI
SDL
OpenAL
NullOutput
RecordingOutput
```

---

# 58. Tại sao nên tách IAudioOutput

Hiện tại `AudioCaptureManager` phụ thuộc trực tiếp:

```cpp
SDL_QueueAudio()
```

Điều này khiến architecture bị khóa vào SDL.

Tách interface:

```text
Audio pipeline
      |
      v
IAudioOutput
      |
 +----+-----+
 |          |
SDL       WASAPI
```

sẽ linh hoạt hơn.

---

# 59. Có nên dùng WASAPI?

Nếu mục tiêu cuối cùng là:

```text
low latency audio player
```

thì về lâu dài có thể cân nhắc:

```text
WASAPI
```

thay vì SDL audio.

Nhưng không cần thay ngay.

Có thể giữ:

```text
SDL Audio
```

cho giai đoạn development.

Sau đó:

```text
IAudioOutput
    |
    +--> SDLAudioOutput
    +--> WASAPIAudioOutput
```

---

# 60. Một vấn đề khác: MPV AO

Kiến trúc:

```cpp
ao=pcm
ao-pcm-file=pipe
```

có thể dùng làm prototype capture.

Nhưng cần kiểm tra kỹ behavior của MPV/PCM AO khi:

- seek.
- pause.
- stop.
- stream reconnect.
- format change.
- audio track change.

Audio transport phải có khả năng:

```text
disconnect
reconnect
reset
flush
```

---

# 61. Khi seek

Ví dụ:

```text
Video:
00:10:00
      |
      | seek
      v
00:20:00
```

Audio pipeline có thể còn:

```text
00:10:00
00:10:01
00:10:02
...
```

trong ring.

Nếu không flush:

```text
old audio
```

có thể tiếp tục được xử lý.

Do đó seek phải:

```text
flush RawRing
flush ProcessedRing
reset sequence
reset timestamp state
```

hoặc đánh dấu generation.

---

# 62. Nên có Media Generation

Ví dụ:

```cpp
uint64_t mediaGeneration;
```

Mỗi lần:

```text
seek
track change
reload
new source
```

tăng:

```text
generation++
```

AudioBlock:

```cpp
struct AudioBlock
{
    uint64_t generation;
    uint64_t sequence;
    double pts;
    ...
};
```

Consumer bỏ block cũ:

```cpp
if (block.generation != currentGeneration)
    discard(block);
```

Điều này rất hữu ích cho player nhiều thread của bạn.

---

# 63. Đây là cách tránh audio cũ sau seek

Thay vì chỉ:

```text
clear()
```

có thể xảy ra race.

Dùng:

```text
generation
```

sẽ giúp downstream biết:

```text
Block belongs to old playback timeline
```

và bỏ nó.

---

# 64. Pause

Khi pause:

```text
Capture có thể tiếp tục hoặc dừng
```

Tùy mục tiêu.

Nếu MPV ngừng xuất audio:

```text
pipe naturally stops receiving data
```

Không cần làm gì quá nhiều.

Output queue nên được kiểm soát để không phát audio sau pause nếu behavior không mong muốn.

---

# 65. Audio track change

Nếu MPV đổi:

```text
language track
```

hoặc:

```text
sample rate
channels
format
```

thì packet header phải thể hiện format.

Consumer phát hiện:

```text
format != current format
```

thì:

```text
flush
reconfigure
```

---

# 66. Raw PCM không nhất thiết phải cố định 48 kHz mãi mãi

Hiện tại:

```cpp
audio-samplerate = 48000
audio-channels = stereo
audio-format = f32le
```

rất phù hợp cho prototype.

Nhưng architecture nên hỗ trợ:

```text
44.1 kHz
48 kHz
96 kHz
mono
stereo
5.1
float
s16
```

Thông tin này nằm trong:

```cpp
AudioFormat
```

---

# 67. AudioFormat

```cpp
struct AudioFormat
{
    uint32_t sampleRate;
    uint16_t channels;
    AudioSampleFormat format;
};
```

`AudioBlock`:

```cpp
struct AudioBlock
{
    AudioFormat format;

    uint32_t frames;

    uint64_t sequence;

    uint64_t generation;

    int64_t ptsUs;

    std::array<float, MaxSamples> samples;
};
```

---

# 68. MaxSamples

Ví dụ:

```cpp
constexpr size_t MaxFrames = 2048;
constexpr size_t MaxChannels = 2;

constexpr size_t MaxSamples =
    MaxFrames * MaxChannels;
```

Nếu cần nhiều channel sau này:

```text
MaxChannels = 8
```

nhưng memory tăng.

---

# 69. Không nên dùng vector resize mỗi block

Tránh:

```cpp
std::vector<float> samples;

samples.resize(...);
```

trong capture loop.

Ưu tiên fixed storage:

```cpp
std::array<float, MaxSamples>
```

hoặc pool.

---

# 70. Memory footprint

Ví dụ:

```text
AudioBlock = 16 KB
Ring = 32 blocks
```

Memory:

```text
16 KB × 32
≈ 512 KB
```

Rất nhỏ.

Có thể có:

```text
RawRing       512 KB
ProcessedRing 512 KB
STTRing       512 KB
Visualizer    512 KB
```

Tổng vẫn chỉ vài MB.

---

# 71. So sánh kiến trúc

## Kiến trúc hiện tại

```text
float
 |
mutex
 |
float
 |
mutex
 |
float
 |
mutex
```

Đặc điểm:

- lock rất nhiều.
- coupling cao.
- SDL nằm trong capture.
- khó mở rộng.
- khó đo latency.
- khó broadcast.

## Kiến trúc mới

```text
AudioBlock
    |
SPSC Ring
    |
Dispatcher
    |
+---+---+---+
|   |   |   |
SDL STT FFT
```

Đặc điểm:

- block-based.
- ít synchronization.
- pipeline rõ.
- dễ mở rộng.
- dễ debug.
- raw/processed độc lập.

---

# 72. Những thứ KHÔNG nên làm

Không nên:

```text
CaptureThread
    |
    +--> STT
    +--> FFT
    +--> SDL
    +--> Filter
    +--> Subtitle
```

Không nên:

```text
RingBuffer<float>
```

Không nên:

```text
mutex per sample
```

Không nên:

```text
CloseHandle(pipe)
```

từ thread khác trong khi thread capture đang sử dụng mà không có ownership protocol.

Không nên:

```text
STT đọc trực tiếp Named Pipe
```

Không nên:

```text
Visualizer đọc trực tiếp Named Pipe
```

Không nên:

```text
500 ms SDL queue
```

nếu mục tiêu là realtime thấp latency.

---

# 73. Thiết kế triển khai theo giai đoạn

## Phase 1 – Sửa bottleneck

Thay:

```text
ThreadSafeRingBuffer<float>
```

bằng:

```text
SpscRingBuffer<AudioBlock>
```

Chưa cần refactor toàn bộ.

Mục tiêu:

```text
Capture
  |
  v
AudioBlock
  |
  v
Ring
```

---

# 74. Phase 2 – Tách SDL

Chuyển:

```text
CaptureLoop
   |
   +--> SDL
```

thành:

```text
Capture
 |
 v
RawRing
 |
 v
AudioOutputThread
 |
 v
SDL
```

---

# 75. Phase 3 – Dispatcher

Thêm:

```text
AudioDispatcher
```

và:

```text
Raw
 |
 +--> Output
 +--> Processing
 +--> Analysis
```

---

# 76. Phase 4 – Processed PCM

Thêm:

```text
AudioProcessor
```

và:

```text
Raw
 |
 v
Processor
 |
 v
Processed
```

---

# 77. Phase 5 – Subtitle

Thêm:

```text
Processed/Raw
 |
 v
STT Worker
 |
 v
Subtitle Timeline
```

Nhớ giữ:

```text
PTS
generation
sequence
```

---

# 78. Phase 6 – Metrics

Thêm:

```text
AudioPipelineMetrics
```

và debug overlay:

```text
Capture:
  23 blocks/s

Raw buffer:
  8 / 32

Processed:
  5 / 32

Dropped:
  0

Underrun:
  0

PTS:
  125.232

Latency:
  37 ms
```

---

# 79. Phase 7 – WASAPI

Chỉ sau khi pipeline ổn định mới cân nhắc:

```text
SDL
 ↓
WASAPI
```

và giữ abstraction:

```cpp
IAudioOutput
```

---

# 80. Thiết kế cuối cùng

Kiến trúc cuối nên là:

```text
                           MPV
                            |
                            v
                     MPV Audio Source
                            |
                            v
                  NamedPipeAudioTransport
                            |
                            v
                    AudioCaptureThread
                            |
                            v
                       AudioBlock
                            |
                            v
                    Raw Audio Stream
                            |
                            v
                     AudioDispatcher
                            |
        +-------------------+-------------------+
        |                   |                   |
        v                   v                   v
   Output Stream       Processing Stream   Analysis Stream
        |                   |                   |
        v                   v                   v
AudioOutputWorker     AudioProcessor        STT Worker
        |                   |                   |
        v                   v                   v
       SDL           Processed PCM Stream   Subtitle
                            |
                     +------+------+
                     |             |
                     v             v
                 Visualizer     Recorder
```

---

# 81. Nguyên tắc kiến trúc quan trọng nhất

## Rule 1

```text
AudioCaptureThread chỉ đọc dữ liệu.
```

## Rule 2

```text
AudioBlock thay cho float.
```

## Rule 3

```text
SPSC thay cho mutex nếu chỉ có 1 producer + 1 consumer.
```

## Rule 4

```text
SDL là consumer, không phải capture layer.
```

## Rule 5

```text
STT không đọc trực tiếp Named Pipe.
```

## Rule 6

```text
Raw PCM và Processed PCM là hai stream độc lập.
```

## Rule 7

```text
Mỗi stream có buffering/drop policy riêng.
```

## Rule 8

```text
Mỗi AudioBlock phải có PTS + sequence + generation.
```

## Rule 9

```text
Seek/track change phải invalidate audio cũ.
```

## Rule 10

```text
Không allocation/locking nặng trên audio fast path.
```

---

# 82. Kết luận

Code hiện tại không sai về mặt ý tưởng cơ bản:

```text
MPV
 ↓
Pipe
 ↓
Capture
 ↓
SDL + Buffer
```

nhưng abstraction đang quá thấp:

```text
sample-oriented
```

và quá nhiều trách nhiệm tập trung trong:

```text
CaptureLoop()
```

Điểm gây giảm hiệu năng lớn nhất là:

```cpp
ThreadSafeRingBuffer<float>
```

kết hợp với:

```cpp
try_push()
```

cho từng sample.

Với:

```text
48 kHz stereo
```

đang có khoảng:

```text
96,000 float/s
```

và tương ứng một lượng rất lớn mutex operations.

Thiết kế nên chuyển sang:

```text
Sample-oriented
        ↓
Block-oriented
```

và:

```text
ThreadSafe mutex queue
        ↓
SPSC lock-free ring
```

Sau đó tách:

```text
Capture
Output
Processing
Analysis
Subtitle
Visualizer
```

thành các stage độc lập.

Kiến trúc cuối cùng:

```text
MPV
 │
 ▼
Named Pipe
 │
 ▼
Capture Thread
 │
 ▼
Raw PCM Stream
 │
 ▼
Dispatcher
 ├─────────────┬──────────────┐
 ▼             ▼              ▼
Output       Processing     Analysis
 │             │              │
 ▼             ▼              ▼
SDL       Processed PCM      STT
               │              │
               ▼              ▼
          Visualizer       Subtitle
```

Đây là kiến trúc phù hợp hơn với mục tiêu dài hạn của `Im_player`, đặc biệt khi bạn muốn sau này dùng cùng audio source cho **subtitle/STT, visualization, audio processing và playback** mà không phải tạo thêm một MPV session chỉ để lấy audio.

## 83. Thông số khởi đầu khuyến nghị

```text
Sample rate:
    48000 Hz

Channels:
    2

Format:
    Float32

Block:
    2048 frames

Block duration:
    ~42.67 ms

Raw ring:
    32 blocks

Processed ring:
    32 blocks

STT ring:
    16–32 blocks

Visualizer ring:
    8–16 blocks

Audio output queue:
    ~50–150 ms

Capture:
    1 thread

Audio output:
    1 thread

Processing:
    1 worker

STT:
    worker riêng

Synchronization:
    SPSC / atomic fast path

Metadata:
    PTS
    sequence
    generation

Drop policy:
    Output: không drop chủ động
    STT: drop oldest
    Visualizer: drop oldest
    Recorder: không drop
```

Đây nên là **baseline architecture** trước khi bạn tối ưu sâu hơn bằng WASAPI hoặc AudioBlockPool.