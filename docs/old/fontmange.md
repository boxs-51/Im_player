Mục tiêu nên là:

Nhiều ImGuiContext + nhiều UIRenderThread được phép render song song, nhưng ImFontAtlas, ImFont, ImFontBaked và quá trình font baking không được phép bị mutate đồng thời.

Đặc biệt sau hai lỗi:

STBTT_assert(pixels[i] == 0);

và:

IM_ASSERT(
    baked->IndexAdvanceX.Size ==
    baked->IndexLookup.Size
);

tôi sẽ thiết kế FontManager theo mô hình Build → Publish → Read-only.

1. Kiến trúc mục tiêu

Không nên:

                  FontManager Singleton
                         │
                    shared atlas
                         │
              ┌──────────┼──────────┐
              ▼          ▼          ▼
          Thread A   Thread B   Thread C
              │          │          │
              └────── mutate ───────┘

Mà:

                       FontManager
                           │
                    FontResourceSet
                           │
                     BUILD PHASE
                           │
                    ┌──────▼──────┐
                    │ FontAtlas   │
                    │ FontBaked   │
                    │ Font data   │
                    └──────┬──────┘
                           │
                       PUBLISH
                           │
                  immutable generation
                           │
             ┌─────────────┼─────────────┐
             ▼             ▼             ▼
          Window A      Window B      Window C
          Context A     Context B     Context C
          Thread A      Thread B      Thread C
             │             │             │
             └────── READ ONLY ──────────┘

Điểm quan trọng nhất:

Render thread không được gọi các API có khả năng build/grow font.

2. Tách FontManager thành 3 trách nhiệm

Tôi không muốn FontManager hiện tại vừa:

load font
build atlas
get font
scale font
manage ImGui

vừa bị tất cả render thread gọi.

Nên tách:

FontManager
│
├── FontCatalog
│      └── font definitions / file / metadata
│
├── FontResourceManager
│      └── build + publish FontResourceSet
│
└── FontView
       └── read-only access cho từng WindowRuntime
3. FontResourceSet

Tạo một object đại diện cho một generation hoàn chỉnh của font system:

struct FontResourceSet
{
    uint64_t generation = 0;

    ImFontAtlas* atlas = nullptr;

    ImFont* defaultFont = nullptr;
    ImFont* uiFont = nullptr;
    ImFont* iconFont = nullptr;

    // Các metadata không mutable trong render phase.
};

Nhưng tôi muốn ownership rõ ràng hơn:

class FontResourceSet
{
public:
    uint64_t Generation() const noexcept
    {
        return m_generation;
    }

    ImFontAtlas* Atlas() const noexcept
    {
        return m_atlas.get();
    }

    ImFont* DefaultFont() const noexcept
    {
        return m_defaultFont;
    }

    ImFont* UIFont() const noexcept
    {
        return m_uiFont;
    }

private:
    friend class FontResourceBuilder;

    uint64_t m_generation = 0;

    std::unique_ptr<ImFontAtlas> m_atlas;

    ImFont* m_defaultFont = nullptr;
    ImFont* m_uiFont = nullptr;
    ImFont* m_iconFont = nullptr;
};

Sau khi publish:

FontResourceSet
        ↓
IMMUTABLE

Không có:

font->Scale = ...
atlas->Build();
atlas->AddFont(...);
atlas->Clear();

từ render thread.

4. FontManager giữ generation

Tôi khuyên:

class FontManager
{
public:
    static FontManager& Instance();

    std::shared_ptr<const FontResourceSet>
    Acquire() const;

    void Rebuild();

private:
    mutable std::mutex m_mutex;

    std::shared_ptr<const FontResourceSet>
        m_current;

    uint64_t m_generation = 0;
};

Lấy resource:

auto fonts = FontManager::Instance().Acquire();

Sau đó:

fonts->DefaultFont();
fonts->UIFont();

Chỉ đọc.

5. Tại sao shared_ptr<const ...> rất phù hợp?

Đây là điểm tôi đặc biệt khuyên dùng.

Render thread:

std::shared_ptr<const FontResourceSet> fonts;

Nó có nghĩa:

Render thread
    │
    └── chỉ có quyền READ

Không thể:

fonts->m_atlas->AddFont(...);

vì resource set bị const.

Đồng thời:

Thread A
    └── shared_ptr generation 10

Thread B
    └── shared_ptr generation 10

có thể tồn tại song song.

Nếu manager rebuild:

generation 11

thì:

FontManager
    │
    ├── current → generation 11
    │
    ├── Thread A → generation 10
    └── Thread B → generation 10

Generation 10 vẫn tồn tại cho đến khi A/B render xong.

Đây là snapshot + immutable resource.

6. Đừng dùng raw pointer làm ownership

Không nên:

ImFont* FontManager::GetFont(...)
{
    return m_activeFont;
}

vì:

FontManager
     │
     └── ImFont*
              ↑
              │
        RenderThread

Nếu FontManager rebuild/destroy atlas:

RenderThread
    ↓
dang giữ ImFont*
    ↓
atlas bị destroy
    ↓
USE-AFTER-FREE

Thay vào đó:

auto fonts = FontManager::Instance().Acquire();

ImFont* font = fonts->UIFont();

fonts giữ lifetime của cả resource generation.

7. Quan trọng: không lưu ImFont* lâu dài trong WindowRuntime

Hiện tại nếu bạn có kiểu:

runtime->resource.font = FontManager::Instance().GetFont(...);

tôi khuyên bỏ.

Thay bằng:

struct WindowFontState
{
    FontId uiFont;
    FontId iconFont;
};

Ví dụ:

enum class FontId
{
    Default,
    UI,
    Icon
};

Window chỉ giữ ID, không giữ raw ImFont*.

8. Render thread resolve font từ resource generation

Ví dụ:

void UIRenderThread::RenderFrame(
    const WindowRenderSnapshot& snapshot)
{
    auto fonts =
        FontManager::Instance().Acquire();

    ImFont* uiFont =
        fonts->Get(FontId::UI);

    ImGui::PushFont(uiFont);

    RenderUI(snapshot);

    ImGui::PopFont();
}

Từ đó:

WindowRuntime
    │
    └── FontId
          │
          ▼
RenderFrame
          │
          ▼
FontResourceSet generation N
          │
          ▼
ImFont*
9. Tuyệt đối không làm ImFont::Scale trong GetFont()

Đây là một điểm tôi sẽ sửa ngay trong FontManager hiện tại của bạn.

Nếu có:

ImFont* FontManager::GetFont(
    FontId id,
    float scale)
{
    ...
    font->Scale = scale;
    return font;
}

thì không thread-safe nếu font shared.

Ví dụ:

Window A
font->Scale = 1.0

Window B
font->Scale = 1.5

Window A
render

Window A có thể render với state mà Window B vừa thay đổi.

10. Font scale phải thuộc WindowRenderSnapshot

Đúng với hướng bạn vừa xây dựng:

struct WindowRenderSnapshot
{
    uint64_t version;

    WindowLayout layout;

    bool visible;
    bool minimized;
    bool fullscreen;

    float fontScale;
};

Render:

const auto& snapshot = ...;

const float scale =
    snapshot.fontScale;

Không mutate:

ImFont::Scale
11. Nhưng có một vấn đề: ImGui không thiết kế ImFont để mỗi window tự ý scale đồng thời

Nếu bạn cần mỗi window có scale khác nhau, có 2 hướng.

Hướng A — Font resource riêng theo scale

Ví dụ:

FontResourceSet
├── UI 100%
├── UI 125%
├── UI 150%
└── UI 175%

Window chỉ chọn:

FontId::UI_125;

Đây là hướng an toàn hơn cho multi-thread.

Hướng B — Dùng ImGui font scaling API phù hợp với version của bạn

Nhưng không được mutate shared ImFont object.

Tôi ưu tiên A nếu hệ thống của bạn cần nhiều window với DPI khác nhau.

12. DPI nên được xử lý bằng FontVariant

Ví dụ:

enum class FontVariant
{
    UI_100,
    UI_125,
    UI_150,
    UI_175,
    UI_200
};

Window snapshot:

struct WindowRenderSnapshot
{
    ...
    FontVariant fontVariant;
};

Render:

auto font = fonts->Get(snapshot.fontVariant);

ImGui::PushFont(font);

Không có:

font->Scale = ...
13. FontResourceBuilder

Build phải nằm ngoài render threads:

class FontResourceBuilder
{
public:
    std::shared_ptr<FontResourceSet>
    Build(const FontConfig& config);
};

Ví dụ:

auto resource =
    builder.Build(config);

Bên trong:

auto atlas =
    std::make_unique<ImFontAtlas>();

atlas->AddFontFromFileTTF(...);
atlas->AddFontFromFileTTF(...);

atlas->Build();

Toàn bộ:

AddFont
Build
Bake
Glyph
STB

xảy ra ở một thread duy nhất.

14. Sau Build mới Publish
void FontManager::Rebuild()
{
    auto newResource =
        m_builder.Build(m_config);

    newResource->m_generation =
        ++m_generation;

    {
        std::lock_guard lock(m_mutex);

        m_current =
            std::move(newResource);
    }
}

Render thread:

auto fonts =
    FontManager::Instance().Acquire();

Nếu nó đang dùng generation cũ:

Thread A
    fonts → generation 10

Manager rebuild:

Manager
    current → generation 11

A vẫn an toàn:

generation 10
    ↓
render xong
    ↓
shared_ptr release
    ↓
destroy
15. Đây là RCU-like architecture

Mô hình này gần giống:

Read-Copy-Update.

Không cần lock render thread lâu.

                 FontManager
                     │
             current resource
                     │
        ┌────────────┴────────────┐
        ▼                         ▼
   Render Thread A           Render Thread B
       Acquire                  Acquire
          │                        │
          ▼                        ▼
      Font v10                  Font v10

             REBUILD
                │
                ▼
             Font v11
                │
                ▼
         Publish atomically

Sau publish:

A → vẫn v10
B → vẫn v10
new frames → v11

Không có frame nào thấy:

Font v10 một nửa
Font v11 một nửa
16. Nhưng ImGui Context cũng cần xử lý đúng

Mỗi UIRenderThread:

ImGuiContext* ctx;

phải thuộc duy nhất một thread tại một thời điểm.

Render:

ImGui::SetCurrentContext(ctx);

và:

Thread A
    → Context A

Thread B
    → Context B

Không được:

Thread A
    → SetCurrentContext(Context B)
17. Shared atlas giữa nhiều context: tôi sẽ thay đổi kiến trúc

Có hai lựa chọn.

Lựa chọn an toàn nhất

Mỗi ImGuiContext có atlas riêng:

Window A
 ├── Context A
 └── Atlas A

Window B
 ├── Context B
 └── Atlas B

Nhưng FontResourceManager vẫn có thể chia sẻ font source/config, không chia sẻ mutable ImFontAtlas.

Đây là cách đơn giản nhất để loại bỏ race.

18. Nếu muốn thật sự shared ImFontAtlas

Có thể:

Shared ImFontAtlas
        │
        ├── Context A
        ├── Context B
        └── Context C

nhưng phải đảm bảo:

Atlas Build/Bake
        ↓
DONE
        ↓
NO MUTATION

Tức là render threads chỉ đọc.

Vấn đề của bạn hiện tại là:

ImFontBaked_BuildGrowIndex()

cho thấy font system của ImGui đang thực hiện lazy mutation.

Do đó với lỗi hiện tại, tôi không khuyên shared atlas mutable.

19. Kiến trúc tôi chọn cho Im_player

Nếu tôi sửa project của bạn, tôi sẽ chọn:

                    FontManager
                        │
                        │ owns
                        ▼
                 FontDefinition
                 FontConfig
                 FontFile
                        │
                        ▼
              FontResourceBuilder
                        │
                   BUILD THREAD
                        │
                        ▼
             FontResourceGeneration
                        │
              immutable after build
                        │
              ┌─────────┼─────────┐
              ▼         ▼         ▼
           Window A  Window B  Window C
              │         │         │
          Context A Context B Context C
              │         │         │
          Thread A  Thread B  Thread C

Và tôi sẽ không share ImFontBaked mutable giữa các render threads.

20. Cấu trúc class đề xuất
using FontGenerationId = uint64_t;

enum class FontId
{
    Default,
    UI,
    Icon
};

struct FontConfig
{
    std::string path;
    float size = 16.0f;
};

struct FontResourceSet
{
    FontGenerationId generation = 0;

    std::unique_ptr<ImFontAtlas> atlas;

    std::unordered_map<FontId, ImFont*> fonts;

    ImFont* Get(FontId id) const noexcept
    {
        auto it = fonts.find(id);

        if (it == fonts.end())
            return nullptr;

        return it->second;
    }
};

Manager:

class FontManager
{
public:
    static FontManager& Instance();

    std::shared_ptr<const FontResourceSet>
    Acquire() const;

    void Initialize(
        const FontConfig& config);

    void Rebuild();

private:
    FontManager() = default;

    mutable std::mutex m_mutex;

    std::shared_ptr<const FontResourceSet>
        m_current;

    std::vector<FontConfig> m_configs;

    FontGenerationId m_generation = 0;
};
21. Acquire() cần cực kỳ đơn giản

Nếu dùng C++20:

std::shared_ptr<const FontResourceSet>
FontManager::Acquire() const
{
    std::lock_guard lock(m_mutex);
    return m_current;
}

Render thread chỉ giữ lock trong thời gian cực ngắn:

Acquire
  ↓
copy shared_ptr
  ↓
unlock
  ↓
render

Không:

lock
 ↓
NewFrame
 ↓
Render
 ↓
Present
 ↓
unlock
22. Nếu muốn giảm lock hơn nữa

Có thể dùng:

std::atomic<std::shared_ptr<const FontResourceSet>>

nếu toolchain/project của bạn phù hợp.

Sau đó:

auto fonts =
    std::atomic_load(&m_current);

Publish:

std::atomic_store(
    &m_current,
    newResource
);

Render thread:

atomic load
    ↓
shared_ptr
    ↓
unlock immediately

Không cần mutex cho read path.

Tuy nhiên tôi sẽ chưa vội dùng atomic shared_ptr. Mutex + shared_ptr là đủ và dễ debug hơn ở giai đoạn hiện tại.

23. WindowRuntime chỉ giữ FontId
struct WindowFontState
{
    FontId defaultFont = FontId::Default;
    FontId uiFont = FontId::UI;
    FontId iconFont = FontId::Icon;
};

Snapshot:

struct WindowRenderSnapshot
{
    uint64_t stateVersion;

    WindowLayout layout;

    bool visible;
    bool minimized;
    bool fullscreen;

    WindowFontState fonts;
};

Render:

void UIRenderThread::RenderFrame(
    const WindowRenderSnapshot& snapshot)
{
    auto fonts =
        FontManager::Instance().Acquire();

    if (!fonts)
        return;

    ImFont* uiFont =
        fonts->Get(snapshot.fonts.uiFont);

    ImGui::PushFont(uiFont);

    RenderUI(snapshot);

    ImGui::PopFont();
}
24. Một invariant cực kỳ quan trọng

Tôi đề nghị thêm comment/contract:

// IMPORTANT:
//
// FontResourceSet is immutable after publication.
//
// Render threads may:
//   - read ImFont*
//   - read ImFontAtlas
//
// Render threads must NOT:
//   - AddFont*
//   - Build()
//   - Clear()
//   - ClearFonts()
//   - mutate ImFont
//   - mutate ImFontBaked
//   - rebuild glyph ranges
//   - modify font scale

Đây sẽ là một architectural rule.

25. Font rebuild phải đi qua một pipeline riêng

Nếu user thay font:

User
 ↓
FontManager::RequestRebuild()
 ↓
FontBuildThread
 ↓
Build FontResourceSet N+1
 ↓
Publish
 ↓
New frames use N+1

Không:

WM_SIZE
 ↓
UIRenderThread
 ↓
FontManager::Build()

Đặc biệt resize không được trực tiếp rebuild shared font.

26. Resize và font scale

Nếu resize chỉ thay:

Window width
Window height
DPI

thì snapshot:

WindowRenderSnapshot

có:

float dpiScale;

nhưng không mutate ImFont.

Nếu DPI thay đổi từ:

100%
→
150%

thì WindowRuntime có thể:

snapshot.fontVariant = UI_150

và render thread lấy:

fonts->Get(FontId::UI_150);

Nếu chưa có variant:

FontManager
   ↓
Build generation N+1
   ↓
Publish
27. Điều này giải quyết cả hai crash bạn vừa gặp
Crash 1
STBTT_assert(pixels[i] == 0);

Không còn nhiều render threads cùng build/bake bitmap.

Crash 2
IndexAdvanceX.Size != IndexLookup.Size

Không còn:

Thread A → resize IndexAdvanceX
Thread B → read IndexLookup

vì baking xảy ra:

FontBuildThread
       │
       ├── IndexAdvanceX
       ├── IndexLookup
       └── complete
             │
             ▼
          Publish

Render chỉ đọc resource hoàn chỉnh.

28. Một vấn đề tôi muốn bạn tránh

Đừng biến FontManager thành:

class FontManager
{
    std::mutex mutex;

    ImFontAtlas atlas;

public:
    ImFont* GetFont()
    {
        std::lock_guard lock(mutex);

        // ...
    }
};

rồi nghĩ:

"Đã thread-safe."

Không đủ.

Nếu:

ImFont*

được trả ra ngoài rồi lock được release:

lock
 ↓
GetFont()
 ↓
unlock
 ↓
ImFont*
 ↓
render

thì object vẫn có thể bị thread khác mutate/destroy.

Lock phải bảo vệ ownership/lifetime hoặc resource phải immutable.

Đó là lý do:

std::shared_ptr<const FontResourceSet>

quan trọng hơn chỉ có std::mutex.

29. Kiến trúc cuối cùng tôi đề xuất
                           ┌─────────────────────┐
                           │     FontManager      │
                           └──────────┬──────────┘
                                      │
                                      │ owns
                                      ▼
                           ┌─────────────────────┐
                           │ FontResourceBuilder  │
                           └──────────┬──────────┘
                                      │
                              BUILD / BAKE
                                      │
                                      ▼
                           ┌─────────────────────┐
                           │ FontResourceSet N    │
                           │                     │
                           │ Atlas               │
                           │ ImFont              │
                           │ ImFontBaked         │
                           │ Glyph data          │
                           └──────────┬──────────┘
                                      │
                                   PUBLISH
                                      │
                                      ▼
                         shared_ptr<const Resource>
                                      │
                    ┌─────────────────┼─────────────────┐
                    │                 │                 │
                    ▼                 ▼                 ▼
               Window A          Window B          Window C
                    │                 │                 │
               Context A         Context B         Context C
                    │                 │                 │
               Thread A          Thread B          Thread C
                    │                 │                 │
                    └──────── READ ONLY ────────────────┘

Và song song với nó:

WindowProc
   │
   ▼
WindowRuntime::state
   │
   ▼
CaptureRenderSnapshot()
   │
   ▼
const WindowRenderSnapshot
   │
   ▼
UIRenderThread::RenderFrame()

Hai hệ thống kết hợp:

                    WindowRenderSnapshot
                       /            \
                      /              \
                     ▼                ▼
              Window geometry      FontId
                     │                │
                     │                ▼
                     │        FontResourceSet
                     │                │
                     └───────┬────────┘
                             ▼
                         ONE FRAME
                             │
                       BeginFrame
                             │
                           Render
                             │
                           Present
30. Thứ tự triển khai thực tế

Tôi sẽ không refactor toàn bộ một lần. Làm theo thứ tự này sẽ an toàn hơn:

Phase 1 — Ngăn mutation nguy hiểm

Tìm và loại bỏ khỏi render thread:

ImFont::Scale = ...

và mọi:

AddFont
Build
Clear
ClearFonts
BuildGrowIndex
Phase 2 — FontManager không trả raw ownership

Đổi:

ImFont* GetFont(...)

thành:

std::shared_ptr<const FontResourceSet>
Acquire()
Phase 3 — WindowRuntime giữ FontId, không giữ ImFont*
FontId uiFont;
Phase 4 — Render snapshot
const WindowRenderSnapshot snapshot
Phase 5 — Build thread

Tách:

FontResourceBuilder

ra khỏi render threads.

Phase 6 — Test multi-window

Test:

1 window
2 windows
3 windows

và:

resize A
resize B
resize A+B cùng lúc
resize liên tục
DPI change
open/close window
Điểm tôi đặc biệt khuyên bạn làm ngay

Trước khi refactor lớn, hãy làm một experiment xác nhận:

static std::mutex g_imguiFontMutex;

chỉ lock phần có khả năng chạm font baking.

Nếu sau đó:

2 windows
resize liên tục

không còn:

STBTT_assert(...)

và:

ImFontBaked_BuildGrowIndex(...)

thì chúng ta gần như đã có bằng chứng chắc chắn rằng shared font state đang bị concurrent mutation.

Sau đó bỏ mutex test và chuyển sang immutable FontResourceSet + build/publish, thay vì giữ mutex trong render loop.

Đây là hướng tôi đánh giá phù hợp nhất với kiến trúc WindowRuntime → UIRenderThread → ImGuiContext của bạn, đồng thời vẫn giữ được khả năng render nhiều cửa sổ song song.