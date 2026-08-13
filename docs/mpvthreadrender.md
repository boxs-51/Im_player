1. Lỗi nguy hiểm nhất nằm ở AcquireFreeBuffer()

Bạn đang có:

// Bước 1
FREE -> RENDERING

sau đó:

// Bước 2
READY -> RENDERING

và đặc biệt:

// Bước 3
if (i != m_currentDisplayIndex) {
    m_frames[i].state.store(BufferState::RENDERING);
    return i;
}

Vấn đề là:

READY không đồng nghĩa với "GPU đã hoàn thành render".

Bạn có fence:

m_frames[index].fence = glFenceSync(...);

nhưng fence chỉ được kiểm tra trong GetStableFrame():

glClientWaitSync(...)

Tức là đang có tình huống:

Render Thread
    |
    | render Frame N
    v
 FBO[0]
    |
    | READY
    v
 UI lấy FBO[0]
    |
    | chưa chắc GPU đã hoàn thành
    |
    +----------------------+
                           |
Render Thread              |
    |                      |
    | AcquireFreeBuffer()  |
    |                      |
    | lấy lại FBO[0]?       |
    v                      |
 render Frame N+1          |
                           |
                           v
                      GPU vẫn đang xử lý

Fence hiện tại chỉ giải quyết:

"Render Thread đã render xong chưa?"

chứ chưa giải quyết:

"UI/GPU đã đọc xong texture này chưa?"

Đây là hai vấn đề hoàn toàn khác nhau.

2. Lỗi nghiêm trọng hơn nằm ở oldDisplay -> FREE

Đây mới là chỗ tôi nghi ngờ mạnh nhất gây ra hiện tượng của bạn.

Trong GetStableFrame():

int oldDisplay = m_currentDisplayIndex.exchange(
    newReadyIndex,
    std::memory_order_acq_rel
);

if (oldDisplay != -1 && oldDisplay != newReadyIndex) {
    m_frames[oldDisplay].state.store(
        BufferState::FREE,
        std::memory_order_release
    );
}

Bạn đang nói với Render Thread:

FBO cũ:
DISPLAYING
      ↓
FREE

ngay khi UI vừa gọi GetStableFrame().

Nhưng thực tế UI mới chỉ lấy texID.

Sau đó:

ImGui::Image(
    static_cast<void*>(frameInfo.texID),
    size,
    ...
);

ImGui::Image() không lập tức đọc texture GPU.

Nó chỉ thêm một ImGui draw command vào command list.

GPU thực sự sample texture có thể xảy ra sau đó khi ImGui render frame.

Do đó timeline thực tế có thể là:

                UI THREAD                         RENDER THREAD

GetStableFrame()
    |
    | FBO[0] = DISPLAYING
    |
    | FBO[1] = READY
    |
    v
currentDisplay = FBO[1]
FBO[0] = FREE
    |
    |---------------------------->
    |                             AcquireFreeBuffer()
    |                             lấy FBO[0]
    |                             |
    |                             render Frame N+2
    |                             |
    |                             ghi texture FBO[0]
    |
ImGui::Image(FBO[0])
    |
    v
GPU sample FBO[0]

Đây là data race ở cấp GPU resource, mặc dù C++ atomic hoàn toàn đúng.

3. Đây chính là lý do mutex trước đó không giải quyết triệt để

Đây cũng giải thích hiện tượng bạn từng nói:

lock render từ Frame Start → Frame End nhưng vẫn nhấp nháy, chỉ ít hơn.

Mutex C++ chỉ bảo vệ:

CPU Thread A
CPU Thread B

Nó không tự bảo vệ:

CPU
 ↓
OpenGL command queue
 ↓
GPU

Ví dụ:

ImGui::Image(texture);

CPU có thể đã chạy xong.

Nhưng GPU vẫn chưa đọc texture.

Nếu Render Thread sau đó:

glBindFramebuffer(...);
mpv_render_context_render(...);

và ghi vào texture đó thì mutex giữa CPU threads không biết GPU còn đang dùng texture.

4. Triple buffer hiện tại chưa phải triple-buffer đúng nghĩa

Bạn có:

std::array<FrameNode, 3> m_frames;

và state:

FREE
RENDERING
READY
DISPLAYING

Ý tưởng này đúng.

Nhưng state machine hiện tại thiếu một trạng thái cực kỳ quan trọng:

DISPLAYING

không có nghĩa:

UI/GPU đã sử dụng xong.

Nó hiện đang bị hiểu gần như:

texture hiện tại được chọn để display.

Hai khái niệm này khác nhau.

Tôi sẽ đổi tư duy state thành:

FREE
  ↓
RENDERING
  ↓
READY
  ↓
DISPLAYING
  ↓
GPU_CONSUMING
  ↓
FREE

hoặc đơn giản hơn:

FREE
RENDERING
READY
DISPLAYING

nhưng DISPLAYING chỉ được chuyển về FREE sau khi GPU hoàn thành việc đọc texture.

5. Fence hiện tại đang đặt sai "phía"

Bạn có:

Render Thread
    |
    | mpv_render_context_render()
    |
    | glFenceSync()
    v
READY

Sau đó UI:

glClientWaitSync()

Fence này trả lời:

"Frame này render xong chưa?"

Nó không trả lời:

"UI/ImGui/GPU đã dùng xong frame cũ chưa?"

Bạn cần consumer fence.

6. Có thể hình dung chính xác bằng 2 fence

Kiến trúc nên là:

                 PRODUCER
              PlayBackRenderThread
                     |
                     |
              mpv_render_context_render
                     |
                     v
                producerFence
                     |
                     v
                   READY
                     |
                     |
                     v
              UI GetStableFrame()
                     |
                     v
                 DISPLAYING
                     |
                     |
                  ImGui
                     |
                     v
               GPU samples
                     |
                     v
                consumerFence
                     |
                     v
                   FREE

Hiện tại bạn chỉ có:

producerFence

Bạn thiếu:

consumerFence
7. Nhưng còn một lỗi nữa: AcquireFreeBuffer() có fallback cực kỳ nguy hiểm

Đoạn:

if (i != m_currentDisplayIndex) {
    m_frames[i].state.store(BufferState::RENDERING);
    return i;
}

đây không phải CAS.

Tức là bạn không xác nhận:

FBO[i] thực sự FREE

Bạn chỉ xác nhận:

FBO[i] != currentDisplayIndex

Ví dụ:

FBO0 = DISPLAYING
FBO1 = READY
FBO2 = RENDERING

thì:

i = 1
i != currentDisplayIndex

và bạn trực tiếp:

READY -> RENDERING

Nếu FBO1 đang là frame mới mà UI chưa sử dụng xong thì frame mới bị ghi đè.

Thậm chí tệ hơn:

FBO0 DISPLAYING
FBO1 READY
FBO2 RENDERING

Render Thread có thể lấy FBO1.

Trong khi UI có thể đang chuẩn bị dùng FBO1.

8. GetStableFrame() cũng có vấn đề về ownership

Bạn làm:

READY -> DISPLAYING

bằng CAS:

compare_exchange_strong(
    expected,
    BufferState::DISPLAYING
)

Điểm này tốt.

Nhưng ngay sau đó:

oldDisplay -> FREE

không có cơ chế nào đảm bảo UI đã xong.

Vì vậy state machine thực tế là:

READY
  ↓
DISPLAYING
  ↓
FREE

trong khi GPU thực tế:

READY
  ↓
UI command recorded
  ↓
GPU chưa sample
  ↓
GPU đang sample
  ↓
GPU xong

Hai lifecycle này không đồng bộ.

9. Đây có thể tạo ra đúng hiện tượng "frame cũ quay lại"

Ví dụ video:

Frame 100
Frame 101
Frame 102
Frame 103

Pool:

FBO0 = 100
FBO1 = 101
FBO2 = 102

UI đang chuẩn bị render:

UI → FBO1 (101)

Render Thread thấy:

FBO0 FREE

ghi:

FBO0 = 103

bình thường.

Nhưng nếu scheduling thay đổi:

UI lấy FBO1
       ↓
FBO1 bị FREE quá sớm
       ↓
Render Thread lấy FBO1
       ↓
FBO1 đang bị ghi Frame 104
       ↓
GPU UI vẫn đang sample FBO1

Kết quả GPU có thể nhìn thấy:

101
104
101
104
103
101

Tùy timing GPU/CPU.

Nó không nhất thiết tạo ra corruption rõ ràng. Nó có thể đơn giản biểu hiện như:

thỉnh thoảng frame cũ xuất hiện lại.

Đặc biệt khi GPU load tăng hoặc FPS/render timing không đều, xác suất sẽ tăng.

10. GetStableFrame() còn có một vấn đề về frame selection

Bạn đang tìm READY:

for (int i = 0; i < 3; ++i) {
    if (CAS READY -> DISPLAYING) {
        newReadyIndex = i;
        break;
    }
}

Không có:

frame sequence number
timestamp
generation

nên UI không biết:

FBO0 = frame 100
FBO1 = frame 101
FBO2 = frame 102

Frame nào mới nhất?

Nó chỉ lấy FBO READY đầu tiên tìm được.

Ví dụ:

FBO0 READY = frame 100
FBO1 READY = frame 102
FBO2 DISPLAYING = frame 101

UI có thể lấy:

frame 100

thay vì:

frame 102

Kết quả nhìn bằng mắt sẽ giống:

frame đang chạy → đột nhiên lùi lại một frame → chạy tiếp.

Đây là một nguyên nhân độc lập thứ hai.

11. Tôi đánh giá các nguyên nhân theo mức độ
Vấn đề	Mức độ	Có thể gây frame cũ?
DISPLAYING -> FREE quá sớm	🔴 Rất cao	Có
Không có consumer GPU fence	🔴 Rất cao	Có
AcquireFreeBuffer() fallback overwrite	🔴 Rất cao	Có
READY không kiểm tra producer fence trước khi reuse	🟠 Cao	Có
Không có frame sequence	🟠 Cao	Có
Chọn READY đầu tiên thay vì newest	🟠 Cao	Có
Mutex CPU	🟡 Thấp	Không giải quyết gốc
mpv_render_context_render()	🟢 Chưa thấy bằng chứng	Chưa thấy
12. Một điểm nữa tôi rất muốn bạn chú ý

Trong Render Thread:

mpv_render_context_render(...);

this->state.fboPool->MarkAsReady(index);

this->state.framerender.store(...);

SDL_Event ev;
ev.type = SDL_MPV_RENDER_UPDATE;
SDLUtils::SDLX_PushUniqueEvent(ev);

framerender.endFrame();

Bạn MarkAsReady() trước khi framerender.endFrame().

Điều này không phải nguyên nhân chính, nhưng nếu FrameTimer hoặc logic timing của bạn có ảnh hưởng đến scheduling thì thứ tự này không đẹp.

Quan trọng hơn:

MarkAsReady()

đã biến FBO thành:

READY

trong khi lifecycle GPU của producer mới chỉ được đánh dấu bằng fence.

13. Kiến trúc tôi khuyên bạn chuyển sang

Tôi sẽ không tiếp tục vá mutex.

Nên chuyển thành Producer → Published → Consumer ownership.

Ví dụ:

                Render Thread
                     │
                     ▼
              Acquire FREE
                     │
                     ▼
                RENDERING
                     │
             mpv render
                     │
                     ▼
              producerFence
                     │
                     ▼
                  READY
                     │
                     ▼
                UI acquire
                     │
                     ▼
               DISPLAYING
                     │
               ImGui::Image
                     │
                     ▼
              GPU consumes
                     │
              consumerFence
                     │
                     ▼
                   FREE

Điểm quan trọng nhất:

Không bao giờ cho Render Thread ghi lại một FBO chỉ vì nó không phải m_currentDisplayIndex.

Nó chỉ được ghi khi state thực sự:

FREE

và nếu GPU ownership liên quan thì phải có fence xác nhận.

14. Đặc biệt: tôi sẽ bỏ fallback này

Đoạn này:

if (i != m_currentDisplayIndex) {
    m_frames[i].state.store(BufferState::RENDERING);
    return i;
}

nên xem là không an toàn.

Thay vào đó:

BufferState expected = BufferState::FREE;

if (m_frames[i].state.compare_exchange_strong(
        expected,
        BufferState::RENDERING,
        std::memory_order_acq_rel))
{
    return i;
}

Nếu không có FREE:

return -1;

Render Thread có thể:

if (index < 0) {
    // bỏ frame này
    // KHÔNG ghi đè frame đang được consumer sử dụng
    continue;
}

Trong video playback, drop một frame mới thường tốt hơn rất nhiều so với:

ghi đè texture mà UI/GPU đang đọc.

15. Và tôi sẽ thêm frameId

Mỗi FrameNode:

uint64_t frameId = 0;

Render:

frame.frameId = ++m_frameCounter;

Sau đó GetStableFrame() chọn:

READY có frameId lớn nhất

thay vì:

READY đầu tiên trong array

Ví dụ:

FBO0 READY frame 105
FBO1 READY frame 107
FBO2 READY frame 106

UI phải lấy:

107

chứ không phải:

105

Điều này sẽ loại bỏ một loại "frame quay ngược" khác.

16. Kết luận

Với code hiện tại, tôi không nghĩ vấn đề chính là ImGui hay mpv.

Điểm đáng nghi nhất là:

             Render Thread
                  │
                  ▼
              FBO READY
                  │
                  ▼
              UI chọn FBO
                  │
                  ▼
          DISPLAYING → FREE
                  │
                  │  ❌ quá sớm
                  ▼
          Render Thread reuse
                  │
                  ▼
           ghi texture mới
                  │
                  ▼
          GPU UI vẫn đọc texture

Trong khi fence hiện tại chỉ bảo vệ:

Render Thread → GPU

chứ chưa bảo vệ:

GPU/ImGui → Render Thread reuse

Đây là ứng viên số 1 cho hiện tượng frame cũ xuất hiện lại theo chu kỳ/ngẫu nhiên.

Ngoài ra còn một lỗi logic riêng:

READY frame cũ
READY frame mới
      ↓
GetStableFrame()
      ↓
lấy READY đầu tiên

nên bạn cũng có khả năng display frame không phải frame mới nhất.