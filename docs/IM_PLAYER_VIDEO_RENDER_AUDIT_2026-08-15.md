# IM_PLAYER --- PHÂN TÍCH TOÀN DIỆN VIDEO RENDER PIPELINE

## 1. Phạm vi

Tài liệu này là phần bổ sung cho Audio Audit, tập trung vào kiến trúc
render video của `Im_player`:

``` text
MPV
 ↓
MPV Render
 ↓
Video Frame
 ↓
FrameBufferPool
 ↓
PlayBackRender
 ↓
PlayBackRenderThread
 ↓
OpenGL
 ↓
SDL Window / ImGui
```

Đồng thời phân tích:

-   OpenGL context ownership
-   SDL window
-   ImGui context
-   MPV render context
-   framebuffer lifecycle
-   frame queue
-   render thread
-   UI thread
-   WindowRuntime
-   nhiều window
-   synchronization
-   stale frame
-   flicker
-   use-after-free
-   GPU resource lifetime
-   shutdown
-   seek
-   resize
-   texture/FBO reuse
-   latency
-   frame dropping

------------------------------------------------------------------------

# 2. Kết luận điều hành

Kiến trúc render hiện tại có nền tảng tốt nhưng đang có một vấn đề kiến
trúc lớn:

> **CPU thread ownership và GPU/OpenGL resource ownership chưa được tách
> đủ rõ.**

Các lỗi kiểu:

``` text
frame cũ xuất hiện
flicker
texture bị thay đổi bất ngờ
render không ổn định
window này ảnh hưởng window kia
```

không nhất thiết xuất phát từ OpenGL drawing code.

Chúng thường xuất phát từ:

``` text
Frame lifetime
+
Framebuffer reuse
+
OpenGL context ownership
+
Render/UI synchronization
+
MPV render callback timing
```

## Mức độ rủi ro

  Khu vực                         Mức độ
  ------------------------------- -------------
  Frame lifetime                  🔴 Critical
  FBO/texture pool ownership      🔴 Critical
  OpenGL context ownership        🔴 Critical
  Multi-window resource sharing   🔴 Critical
  Render snapshot                 🔴 Critical
  Stale frame prevention          🔴 Critical
  Resize synchronization          🟠 High
  MPV render callback             🟠 High
  Frame dropping                  🟠 High
  GPU synchronization             🟠 High
  ImGui integration               🟠 High
  Shutdown                        🔴 Critical
  Latency                         🟠 High

------------------------------------------------------------------------

# 3. Pipeline nên được hiểu thành 4 tầng

Không nên coi toàn bộ video render là một pipeline duy nhất.

Nên phân chia:

``` text
                 MEDIA DOMAIN

MPV Decoder
    │
    ▼
MPV Video Frame
    │
    ▼

                 FRAME DOMAIN

FrameQueue / FrameSnapshot
    │
    ▼

                 GPU DOMAIN

OpenGL Texture
    │
    ▼
FBO
    │
    ▼
Shader
    │
    ▼

                 WINDOW DOMAIN

SDL Window
    │
    ▼
ImGui / Present
```

Mỗi tầng phải có ownership riêng.

------------------------------------------------------------------------

# 4. Ownership hiện tại cần được chuẩn hóa

Mô hình mục tiêu:

``` text
MPV Thread
    │
    │ produces frame metadata
    ▼
FrameBridge
    │
    ▼
RenderThread
    │
    ├── owns OpenGL context
    ├── owns texture/FBO
    ├── owns framebuffer pool
    └── performs all GPU operations
              │
              ▼
          SDL Present
```

UI thread chỉ gửi:

``` text
resize
visibility
layout
seek command
render configuration
```

UI thread không trực tiếp:

``` text
glBindTexture
glDeleteTextures
glDeleteFramebuffers
glTexSubImage2D
```

nếu resource thuộc RenderThread.

------------------------------------------------------------------------

# 5. Vấn đề lớn nhất: Frame lifetime

Một frame có thể trải qua:

``` text
MPV
 ↓
CPU wrapper
 ↓
framebuffer
 ↓
texture
 ↓
render
```

Nếu frame/resource bị recycle trước khi GPU hoàn thành:

``` text
RenderThread
   │
   ├── sử dụng Texture A
   │
GPU vẫn chưa xong
   │
   └── Pool trả Texture A
             ↓
        frame mới ghi vào A
```

thì frame đang render có thể bị thay đổi giữa chừng.

Kết quả:

``` text
flicker
tearing-like artifact
old/new frame mixing
random corruption
```

------------------------------------------------------------------------

# 6. FrameBufferPool không nên chỉ dựa vào free-list

Một pool:

``` text
free → in use → free
```

là chưa đủ cho GPU.

Cần phân biệt:

``` text
CPU available
GPU available
```

Một resource có thể:

``` text
CPU finished
```

nhưng:

``` text
GPU still using it
```

Do đó pool nên có trạng thái:

``` cpp
enum class FrameSlotState {
    Free,
    Writing,
    Ready,
    Rendering,
    GpuPending
};
```

------------------------------------------------------------------------

# 7. RenderSnapshot là abstraction rất quan trọng

Đối với kiến trúc nhiều window, không nên để RenderThread đọc trực tiếp
hàng loạt state mutable.

Nên tạo:

``` cpp
struct VideoRenderSnapshot {
    uint64_t generation;
    uint64_t frameSequence;

    GLuint texture;
    GLuint framebuffer;

    int width;
    int height;

    double pts;

    bool visible;
    bool valid;

    uint64_t resizeGeneration;
};
```

RenderThread chỉ render snapshot.

UI thread chỉ publish snapshot/configuration.

------------------------------------------------------------------------

# 8. Không render trực tiếp từ mutable FrameBufferPool state

Một anti-pattern:

``` cpp
auto& slot = pool.acquire();

render(slot);

pool.release(slot);
```

nếu `release()` xảy ra trước khi GPU command hoàn tất.

Nên:

``` text
Acquire
 ↓
Upload
 ↓
Publish Ready
 ↓
RenderThread takes ownership
 ↓
Draw
 ↓
GPU fence
 ↓
GpuPending
 ↓
Free
```

------------------------------------------------------------------------

# 9. GPU fence

Nếu resource được recycle nhanh, dùng:

``` cpp
GLsync fence = glFenceSync(
    GL_SYNC_GPU_COMMANDS_COMPLETE,
    0
);
```

Sau đó slot chỉ trở lại Free khi:

``` cpp
glClientWaitSync(...)
```

xác nhận GPU đã hoàn thành.

Không nên dùng:

``` cpp
glFinish();
```

cho mỗi frame.

`glFinish()` sẽ phá latency/performance.

------------------------------------------------------------------------

# 10. Double buffering chưa chắc đủ

Hai framebuffer:

``` text
A
B
```

có thể vẫn thiếu.

Nếu:

``` text
CPU upload
GPU render
GPU presentation
```

cùng chồng lên nhau, triple buffering thường an toàn hơn:

``` text
A = Rendering
B = Ready
C = Uploading
```

Đối với video realtime, có thể dùng:

``` text
3–4 GPU frame slots
```

và drop frame cũ khi backlog.

------------------------------------------------------------------------

# 11. Frame dropping phải có chủ đích

Không nên:

``` text
Frame 100
Frame 101
Frame 102
Frame 103
...
```

rồi cố render toàn bộ.

Video realtime cần:

``` text
latest frame wins
```

Ví dụ:

``` text
Queue:
100
101
102
103
```

nếu render deadline đã ở 103:

``` text
drop 100
drop 101
drop 102
render 103
```

Điều này giảm latency.

------------------------------------------------------------------------

# 12. Nhưng không được drop frame đang được GPU sử dụng

Đây là điểm quan trọng.

Có thể drop:

``` text
Ready
```

nhưng không được drop:

``` text
Rendering
GpuPending
```

Do đó frame queue cần state machine.

------------------------------------------------------------------------

# 13. Generation cho video

Audio cần generation.

Video cũng cần generation.

Ví dụ:

``` text
Play video A
generation = 10

Seek
generation = 11

Video A old frame:
generation = 10
```

RenderThread phải:

``` cpp
if (frame.generation != currentGeneration)
    discard(frame);
```

Điều này ngăn:

``` text
seek → frame cũ xuất hiện
```

------------------------------------------------------------------------

# 14. Seek pipeline

Đề xuất:

``` text
UI
 │
 ▼
Seek Command
 │
 ▼
MPV
 │
 └── seek
      │
      ▼
generation++
      │
      ├── invalidate CPU frames
      ├── invalidate Ready frames
      └── reset presentation timing
                    │
                    ▼
               first new frame
```

Không để frame cũ từ queue tiếp tục đi tới OpenGL.

------------------------------------------------------------------------

# 15. Resize pipeline

Resize hiện tại là một nguồn race tiềm năng.

Không nên:

``` text
UI thread
 ↓
resize OpenGL texture
```

trong khi:

``` text
RenderThread
 ↓
đang render texture
```

Nên:

``` text
UI
 ↓
ResizeCommand
 ↓
RenderThread
 ↓
recreate FBO/texture
```

------------------------------------------------------------------------

# 16. Resize generation

Thêm:

``` cpp
uint64_t resizeGeneration;
```

Snapshot:

``` text
frame.resizeGeneration = 10
window.resizeGeneration = 11
```

thì:

``` text
frame discard
```

và RenderThread tạo resource mới.

Điều này tránh render vào FBO cũ sau resize.

------------------------------------------------------------------------

# 17. OpenGL context ownership

Một nguyên tắc cần áp dụng tuyệt đối:

> Một OpenGL resource phải được thao tác trong context có
> ownership/share-group phù hợp và phải có lifecycle rõ ràng.

Không nên để:

``` text
UIThread
RenderThread
MPVRenderThread
```

cùng tùy ý thao tác cùng texture/FBO.

------------------------------------------------------------------------

# 18. Multi-window

Với nhiều window:

``` text
Window A
Window B
Window C
```

không nên có:

``` text
Global shared framebuffer
```

mà không có ownership.

Mỗi window nên có:

``` text
WindowRenderContext
    ├── SDL_Window
    ├── GLContext
    ├── ImGuiContext
    ├── framebuffer resources
    └── render snapshot
```

Nếu resource sharing giữa context được dùng, phải quy định rõ:

``` text
Shared immutable resource
```

khác với:

``` text
Window-local render target
```

------------------------------------------------------------------------

# 19. Font/ImGui và video texture phải tách resource domain

Không nên để:

``` text
ImGuiContext A
```

và:

``` text
ImGuiContext B
```

tùy ý dùng cùng mutable atlas/resource nếu lifecycle không được đồng bộ.

Video texture cũng vậy.

Kiến trúc tốt:

``` text
WindowRenderContext
    │
    ├── ImGui resources
    ├── video render resources
    └── window-local state
```

Global:

``` text
Shared immutable assets
```

chỉ khi context sharing được thiết kế rõ.

------------------------------------------------------------------------

# 20. MPV OpenGL render integration

MPV render callback không nên trực tiếp thao tác UI.

Nó nên:

``` text
MPV
 ↓
request redraw
 ↓
Render scheduler
 ↓
RenderThread
 ↓
mpv_render_context_render()
```

hoặc kiến trúc tương đương phù hợp với API đang dùng.

Điểm quan trọng là callback:

``` text
không được
 ├── destroy texture
 ├── resize framebuffer
 ├── mutate ImGui
 └── touch WindowManager
```

nếu callback đang chạy ở thread khác.

------------------------------------------------------------------------

# 21. `mpv_render_context` lifetime

Phải đảm bảo:

``` text
mpv_render_context_create
        ↓
render thread initialized
        ↓
render
        ↓
stop rendering
        ↓
destroy render context
        ↓
destroy mpv handle
```

Không được:

``` text
destroy mpv_handle
       ↓
render thread vẫn gọi render
```

Đây là shutdown race rất nguy hiểm.

------------------------------------------------------------------------

# 22. RenderThread phải có explicit lifecycle

Nên:

``` cpp
enum class RenderThreadState {
    Created,
    Starting,
    Running,
    Paused,
    Reconfiguring,
    Stopping,
    Stopped
};
```

Không dùng nhiều atomic bool rời rạc như:

``` text
running
stop
rendering
initialized
ready
shutdown
```

mà không có state transition rõ.

------------------------------------------------------------------------

# 23. Một owner cho OpenGL

Mục tiêu:

``` text
RenderThread
    owns:
        GLContext
        FrameBufferPool
        VideoTextures
        ShaderProgram
        VAO/VBO
        MPV render context
```

UI thread:

``` text
commands only
```

MPV event thread:

``` text
events only
```

------------------------------------------------------------------------

# 24. Render command queue

Nên có:

``` cpp
enum class RenderCommandType {
    Resize,
    SetVisibility,
    SetGeneration,
    Reconfigure,
    Shutdown
};
```

UI:

``` cpp
renderCommands.push(...)
```

RenderThread:

``` cpp
while (...) {
    processCommands();
    renderFrame();
}
```

Điều này giải quyết rất nhiều race.

------------------------------------------------------------------------

# 25. Frame queue nên là latest-frame queue

Không nhất thiết phải dùng FIFO dài.

Video render thường phù hợp:

``` text
LatestFrameSlot
```

hoặc:

``` text
SPSC ring capacity 3–4
```

với policy:

``` text
drop old Ready frames
```

nhưng giữ:

``` text
Rendering/GpuPending
```

------------------------------------------------------------------------

# 26. Video frame timing

Render không nên chỉ:

``` text
while(render)
    drawLatestFrame();
```

Cần dựa vào:

``` text
frame PTS
current playback clock
display time
```

Ví dụ:

``` cpp
if (frame.pts <= videoClock + tolerance)
    present(frame);
else
    wait;
```

Nếu frame quá cũ:

``` cpp
drop
```

------------------------------------------------------------------------

# 27. Clock architecture

Nên có:

``` text
PlaybackClock
      │
      ├── AudioClock
      │
      └── VideoClock
```

hoặc:

``` text
MasterClock
   ├── Audio
   └── Video
```

Không nên mỗi pipeline tự lấy:

``` text
time-pos
steady_clock
frame arrival time
```

rồi so sánh trực tiếp.

------------------------------------------------------------------------

# 28. Audio/video synchronization

Nếu Audio là master:

``` text
AudioClock
     │
     ▼
Video render deadline
```

Video:

``` text
videoPTS - audioClock
```

### Nếu video chậm

``` text
render latest frame
```

### Nếu video quá sớm

``` text
wait
```

### Nếu video quá trễ

``` text
drop frame
```

------------------------------------------------------------------------

# 29. Flicker --- các nguyên nhân cần ưu tiên

Trong kiến trúc hiện tại, khi thấy flicker, kiểm tra theo thứ tự:

``` text
1. FrameBufferPool reuse
2. texture reuse
3. GPU resource lifetime
4. render snapshot
5. OpenGL context ownership
6. resize
7. MPV frame callback
8. stale frame generation
9. swap/present timing
10. ImGui draw ordering
```

Không nên chỉ thêm mutex vào toàn bộ render loop.

Mutex có thể giảm triệu chứng nhưng không sửa ownership.

------------------------------------------------------------------------

# 30. Vì sao lock toàn bộ RenderStart → RenderEnd không giải quyết triệt để

Một mutex:

``` cpp
lock();
render();
unlock();
```

chỉ bảo vệ:

``` text
CPU threads
```

Không tự động bảo vệ:

``` text
GPU commands
GPU resource lifetime
MPV callback
frame pool reuse
SDL present
```

Do đó hiện tượng flicker giảm nhưng vẫn còn là hoàn toàn có thể xảy ra.

------------------------------------------------------------------------

# 31. FramePool nên quản lý resource chứ không quản lý "frame logic"

Tách:

``` text
FrameData
```

khỏi:

``` text
GpuFrameResource
```

Ví dụ:

``` cpp
struct VideoFrameData {
    uint64_t generation;
    uint64_t sequence;
    double pts;
};

struct GpuFrameResource {
    GLuint texture;
    GLuint fbo;
    GLsync fence;
};
```

Sau đó:

``` text
VideoFrame
    ├── metadata
    └── GPU resource
```

được quản lý rõ.

------------------------------------------------------------------------

# 32. Không nên giữ raw pointer tới pooled frame quá lâu

Anti-pattern:

``` cpp
Frame* current = pool.acquire();
m_currentFrame = current;
pool.release(current);
```

Sau đó:

``` cpp
render(m_currentFrame);
```

=\> use-after-recycle.

Nên dùng:

``` text
slot state
```

hoặc:

``` text
FrameHandle
```

có ownership.

------------------------------------------------------------------------

# 33. FrameHandle

Một thiết kế tốt:

``` cpp
struct FrameHandle {
    uint32_t slot;
    uint64_t generation;
    uint64_t sequence;
};
```

RenderThread validate:

``` cpp
if (!pool.isValid(handle))
    discard;
```

Không truyền raw pointer xuyên thread.

------------------------------------------------------------------------

# 34. GPU/CPU synchronization

Không nên:

``` cpp
glFinish();
```

mỗi frame.

Không nên:

``` cpp
glFlush();
```

và giả định GPU đã xong.

Nếu cần reuse resource:

``` cpp
glFenceSync
```

là abstraction phù hợp.

------------------------------------------------------------------------

# 35. Double/triple buffering recommendation

Cho video:

``` text
3 GPU slots
```

là baseline hợp lý.

Ví dụ:

``` text
Slot 0 = Rendering
Slot 1 = Ready
Slot 2 = Uploading
```

Nếu GPU latency cao:

``` text
4 slots
```

nhưng không nên tạo queue vô hạn vì latency tăng.

------------------------------------------------------------------------

# 36. Texture upload

Nếu video upload CPU → GPU:

``` text
glTexSubImage2D
```

nên:

-   texture allocation một lần;
-   reuse texture;
-   tránh `glTexImage2D` mỗi frame;
-   tránh allocation trong render loop;
-   kiểm tra stride/pixel format;
-   kiểm tra alignment.

Nếu MPV có thể render trực tiếp qua GPU path thì ưu tiên path đó để
tránh CPU copy.

------------------------------------------------------------------------

# 37. Pixel format

Phải phân biệt:

``` text
YUV
RGB
RGBA
NV12
P010
```

Không nên convert CPU nếu GPU shader có thể xử lý.

Kiến trúc:

``` text
MPV frame
 ↓
GPU texture
 ↓
YUV shader
 ↓
RGB output
```

thường tốt hơn:

``` text
MPV
 ↓
CPU RGB conversion
 ↓
upload
```

------------------------------------------------------------------------

# 38. Shader ownership

Shader program nên thuộc:

``` text
RenderThread / GL context
```

Không compile shader từ UI thread rồi tùy ý dùng ở render thread nếu
context/share-group chưa được thiết kế.

Shader:

``` text
compile once
link once
reuse
```

Không compile trong frame loop.

------------------------------------------------------------------------

# 39. FBO completeness

Sau khi tạo:

``` cpp
glFramebufferTexture2D(...)
```

phải kiểm tra:

``` cpp
glCheckFramebufferStatus(GL_FRAMEBUFFER)
```

Không nên giả định FBO luôn complete.

Đặc biệt sau:

``` text
resize
format change
context recreation
```

------------------------------------------------------------------------

# 40. OpenGL state leakage

Render code phải hạn chế phụ thuộc state còn lại từ ImGui/MPV.

Ví dụ:

``` text
GL_BLEND
GL_SCISSOR_TEST
GL_DEPTH_TEST
GL_FRAMEBUFFER
GL_TEXTURE
GL_VIEWPORT
```

Nên render pass thiết lập explicit state cần thiết.

Không dựa vào:

``` text
"frame trước đã set rồi"
```

------------------------------------------------------------------------

# 41. Viewport

Resize:

``` cpp
glViewport(0, 0, width, height);
```

phải thuộc RenderThread.

Không để UI thread gọi `glViewport()`.

------------------------------------------------------------------------

# 42. Present

SDL:

``` text
Render
 ↓
ImGui
 ↓
SDL_GL_SwapWindow
```

nên được thực hiện bởi thread sở hữu GL context.

Không:

``` text
RenderThread → draw
UIThread → SwapWindow
```

------------------------------------------------------------------------

# 43. VSync

VSync phải được coi là một phần của presentation policy:

``` text
Immediate
VSync
Adaptive
```

Không thay đổi từ thread khác mà không có command.

Nếu VSync làm tăng latency:

``` text
triple buffering
```

có thể làm vấn đề tệ hơn.

Cần đo thay vì đoán.

------------------------------------------------------------------------

# 44. WindowRuntime

WindowRuntime nên sở hữu:

``` text
SDL_Window
GLContext
RenderThread
ImGuiContext
WindowRenderSnapshot
```

Không nên để:

``` text
WindowManager
```

tùy ý lấy raw pointer tới OpenGL resource của window.

------------------------------------------------------------------------

# 45. Multi-window isolation

Mỗi window:

``` text
Window A
 ├── GL context A
 ├── RenderThread A
 ├── ImGui A
 └── FramePool A

Window B
 ├── GL context B
 ├── RenderThread B
 ├── ImGui B
 └── FramePool B
```

Nếu dùng shared context:

``` text
Shared immutable resources
```

chỉ nên chứa:

``` text
shader
font
static texture
```

Còn:

``` text
FBO
backbuffer
window render target
```

nên window-local.

------------------------------------------------------------------------

# 46. Context switching

Nếu một thread quản lý nhiều GL contexts:

``` text
SDL_GL_MakeCurrent(A)
render A

SDL_GL_MakeCurrent(B)
render B
```

phải có scheduling rõ.

Không nên:

``` text
thread A context A
thread B context B
```

nhưng cùng sửa shared mutable GL resource mà không synchronization.

------------------------------------------------------------------------

# 47. Context destruction

Shutdown đúng:

``` text
stop render
 ↓
join render thread
 ↓
make context current
 ↓
destroy GPU resources
 ↓
destroy ImGui backend
 ↓
destroy ImGui context
 ↓
destroy SDL_GLContext
 ↓
destroy SDL_Window
```

Không:

``` text
SDL_Window destroyed
 ↓
RenderThread still running
```

------------------------------------------------------------------------

# 48. Stale frame protection

Mỗi frame nên có:

``` cpp
generation
sequence
pts
```

RenderThread giữ:

``` cpp
currentGeneration
```

Nếu:

``` cpp
frame.generation < currentGeneration
```

thì:

``` text
drop
```

Nếu:

``` cpp
frame.generation > currentGeneration
```

thì có thể update generation tùy state machine.

------------------------------------------------------------------------

# 49. Sequence checking

RenderThread có:

``` cpp
lastSequence
```

Nếu:

``` text
seq <= lastSequence
```

thì frame cũ/duplicate.

Log:

``` text
STALE VIDEO FRAME
```

Nếu:

``` text
seq > lastSequence + 1
```

thì có frame bị drop.

Đây là metric rất quan trọng.

------------------------------------------------------------------------

# 50. Frame latency metrics

Nên đo:

``` text
decodePTS
captureTime
uploadTime
renderStart
renderEnd
presentTime
```

Sau đó:

``` text
captureLatency
GPUUploadLatency
RenderLatency
PresentationLatency
EndToEndLatency
```

Không thể tối ưu latency đáng tin nếu chỉ đo FPS.

------------------------------------------------------------------------

# 51. FPS không phải metric duy nhất

Một video có:

``` text
60 FPS
```

nhưng:

``` text
500 ms latency
```

vẫn là render pipeline tệ cho interactive player.

Nên đo:

``` text
FPS
frame drops
frame repeats
jitter
latency
GPU time
CPU time
queue depth
```

------------------------------------------------------------------------

# 52. Frame pacing

Không nên chỉ:

``` cpp
sleep(16ms);
```

với video 60fps.

Frame pacing phải dựa trên:

``` text
PTS
display clock
VSync
```

Nếu refresh rate là:

``` text
144Hz
```

video:

``` text
24fps
```

cần scheduling chính xác.

------------------------------------------------------------------------

# 53. Frame repeat

Không phải lúc nào không có frame mới cũng là lỗi.

Nếu:

``` text
24fps video
60Hz display
```

nhiều display intervals phải render cùng frame.

Do đó:

``` text
No new frame
```

không đồng nghĩa:

``` text
black screen
```

Render pipeline nên giữ:

``` text
last valid frame
```

cho đến khi frame mới đến.

------------------------------------------------------------------------

# 54. Nhưng phải phân biệt last frame và stale generation

Cho phép:

``` text
same generation:
repeat last frame
```

Không cho:

``` text
old generation:
repeat forever after seek
```

Sau seek:

``` text
invalidate presentation frame
```

hoặc cho phép giữ frame cũ trong thời gian rất ngắn tùy UX.

------------------------------------------------------------------------

# 55. Black frame policy

Khi:

``` text
seek
track change
new media
```

có hai policy:

### Policy A

Hiển thị frame cũ đến khi frame mới đến.

Ưu:

``` text
không nhấp nháy đen
```

### Policy B

Clear video surface.

Ưu:

``` text
không hiển thị frame cũ
```

Với player có subtitle/STT, Policy A thường dễ chịu hơn nhưng phải có
generation để không giữ frame cũ vô hạn.

------------------------------------------------------------------------

# 56. Error recovery

Render error:

``` text
GL context lost/recreated
MPV render failure
SDL window failure
```

không nên crash toàn bộ PlayerSession ngay.

Nên:

``` text
RenderError
 ↓
stop GPU operations
 ↓
recreate resource
 ↓
resume current generation
```

------------------------------------------------------------------------

# 57. Logging

Mỗi frame debug log nên có:

``` text
windowId
generation
sequence
pts
textureId
fboId
slot
state
queueDepth
```

Ví dụ:

``` text
VIDEO
window=main
gen=21
seq=8821
pts=120.833
slot=2
texture=17
state=Rendering
queue=1
```

Khi flicker xảy ra có thể truy nguyên chính xác.

------------------------------------------------------------------------

# 58. Các anti-pattern cần loại bỏ

``` text
❌ UI thread render OpenGL
❌ UI thread release pooled frame
❌ nhiều thread cùng mutate texture
❌ global framebuffer dùng cho nhiều window
❌ raw pointer tới pooled frame
❌ glFinish mỗi frame
❌ recreate texture mỗi frame
❌ resize từ UI vào GPU trực tiếp
❌ render stale generation
❌ FIFO frame queue quá dài
❌ mutex toàn bộ render loop
❌ destroy SDL window trước render thread
```

------------------------------------------------------------------------

# 59. Kiến trúc RenderSnapshot hoàn chỉnh

``` cpp
struct WindowRenderSnapshot {
    uint64_t generation;
    uint64_t frameSequence;

    uint64_t resizeGeneration;

    double pts;

    int viewportWidth;
    int viewportHeight;

    GLuint videoTexture;

    bool hasFrame;
    bool visible;

    float displayAspectRatio;
};
```

Snapshot không sở hữu texture nếu texture thuộc RenderThread.

Nếu snapshot cần ownership:

``` text
FrameHandle
```

thay vì raw GLuint.

------------------------------------------------------------------------

# 60. Render command model

``` cpp
struct RenderCommand {
    enum class Type {
        Resize,
        SeekGeneration,
        SetVisible,
        Reconfigure,
        Shutdown
    };

    Type type;

    uint64_t generation;
    uint64_t resizeGeneration;

    int width;
    int height;
};
```

UI:

``` text
command
```

RenderThread:

``` text
consume command
```

------------------------------------------------------------------------

# 61. Pipeline mục tiêu

``` text
                         MPV
                          │
                          ▼
                  MPV Render Context
                          │
                          ▼
                    FrameBridge
                          │
                          ▼
                  VideoFrameQueue
                          │
                          ▼
                  RenderThread ONLY
                          │
             ┌────────────┴────────────┐
             │                         │
             ▼                         ▼
        FrameBufferPool            WindowState
             │                         │
             ▼                         │
       GPU Texture/FBO                 │
             │                         │
             └────────────┬────────────┘
                          ▼
                     Video Pass
                          │
                          ▼
                       ImGui
                          │
                          ▼
                    SDL SwapWindow
```

------------------------------------------------------------------------

# 62. Kiến trúc kết hợp Audio + Video

Sau khi sửa Audio Audit và Video Render Audit:

``` text
                         MPV
                    ┌─────┴─────┐
                    │           │
                  Audio        Video
                    │           │
                    ▼           ▼
                 Capture    MPV Render
                    │           │
                    ▼           ▼
                Processor    FrameBridge
                 │   │           │
                 │   └──► STT    ▼
                 │            VideoQueue
                 ▼                │
              Playback            ▼
                 │            RenderThread
                 ▼                │
             AudioDevice          ▼
                              SDL/ImGui
```

Cả hai dùng chung:

``` text
generation
media identity
playback clock
seek protocol
shutdown protocol
```

nhưng **không dùng chung buffer ownership**.

------------------------------------------------------------------------

# 63. P0 --- Render phải sửa trước

``` text
[ ] Một RenderThread duy nhất sở hữu GL resources của mỗi window
[ ] Không UI thread thao tác GPU resource
[ ] FramePool có explicit slot state
[ ] Không recycle GPU resource khi GPU còn dùng
[ ] Thêm generation
[ ] Thêm sequence
[ ] Seek invalidates old video frames
[ ] Resize chạy trên RenderThread
[ ] Shutdown join RenderThread trước khi destroy Window/GL
[ ] Không raw pointer pooled frame xuyên thread
[ ] Multi-window không dùng chung mutable FBO
[ ] Frame queue có bounded capacity
[ ] Drop Ready frame cũ, không drop Rendering frame
```

------------------------------------------------------------------------

# 64. P1 --- Performance

``` text
[ ] Triple buffering
[ ] GLsync fence
[ ] Reuse texture
[ ] Reuse FBO
[ ] Không allocation trong render loop
[ ] Latest-frame policy
[ ] GPU/CPU timing
[ ] Frame pacing
[ ] VSync measurement
[ ] queue latency metrics
```

------------------------------------------------------------------------

# 65. P2 --- Advanced

``` text
[ ] Zero-copy / GPU-native MPV render path
[ ] YUV shader
[ ] HDR / 10-bit
[ ] hardware decoder integration
[ ] multi-monitor refresh-rate handling
[ ] adaptive frame pacing
[ ] GPU context recovery
```

------------------------------------------------------------------------

# 66. Test matrix

## Basic

``` text
Play local file
Play URL
Pause
Resume
Stop
```

## Seek

``` text
Seek forward
Seek backward
Rapid seek
Seek while paused
Seek while buffering
```

## Window

``` text
Resize
Minimize
Restore
Fullscreen
Move monitor
Change DPI
```

## Multi-window

``` text
Open A
Open B
Render both
Close A
Continue B
Close B
```

## Stress

``` text
Resize + Seek
Seek + Pause
TrackChange + Resize
WindowClose + Playback
BackendChange + Render
Shutdown during seek
```

## Frame correctness

``` text
No stale generation
No duplicate sequence
No invalid texture
No FBO incomplete
No use-after-free
No GL calls after context destruction
```

------------------------------------------------------------------------

# 67. Debug assertions

Trong Debug nên có:

``` cpp
assert(renderThreadId == std::this_thread::get_id());
```

trước mọi GPU operation quan trọng.

Ví dụ:

``` cpp
void FrameBufferPool::Acquire() {
    assert(IsRenderThread());
}
```

Tương tự:

``` cpp
VideoTexture::Destroy()
```

``` cpp
assert(IsRenderThread());
```

Điều này bắt race rất sớm.

------------------------------------------------------------------------

# 68. Kết luận

Vấn đề render của `Im_player` không nên được giải quyết bằng cách:

``` text
thêm mutex
```

một cách đại trà.

Cần chuyển sang mô hình:

``` text
ONE OWNER
ONE THREAD
EXPLICIT LIFETIME
EXPLICIT FRAME STATE
EXPLICIT GENERATION
EXPLICIT GPU SYNC
```

Cụ thể:

``` text
UI
 │
 ▼
RenderCommandQueue
 │
 ▼
RenderThread
 │
 ├── GLContext
 ├── MPVRenderContext
 ├── FrameBufferPool
 ├── VideoTextures
 ├── FrameQueue
 └── Present
```

Đây là kiến trúc phù hợp nhất để giải quyết đồng thời:

``` text
flicker
stale frame
use-after-free
multi-window interference
resize race
seek race
GPU resource corruption
render latency
```

------------------------------------------------------------------------

# 69. Thứ tự triển khai khuyến nghị

``` text
Phase 1
Frame ownership
      ↓
Phase 2
RenderThread GPU ownership
      ↓
Phase 3
Generation + Seek invalidation
      ↓
Phase 4
Resize command
      ↓
Phase 5
FramePool state machine
      ↓
Phase 6
GPU fence
      ↓
Phase 7
Frame pacing
      ↓
Phase 8
Multi-window isolation
      ↓
Phase 9
GPU-native MPV rendering
```

**Không nên tối ưu GPU trước khi Phase 1--4 hoàn thành.**

Nếu ownership còn sai thì mọi tối ưu FPS/latency đều có thể chỉ làm lỗi
race khó tái hiện hơn.
