# IP-BUILD-001 — Canonical Build, Dependency & Validation Policy

> **Policy ID:** IP-BUILD-001  
> **Version:** 1.0.0  
> **Parent authority:** [IP-POL-001](./PROJECT_EXECUTION_POLICY.md)  
> **Status:** CANONICAL after merge to `main`

## 1. Purpose

Policy này định nghĩa điều kiện để một commit của `Im_player` được gọi là:

- CONFIGURABLE;
- BUILD PASS;
- RUNTIME SMOKE PASS;
- CI PASS;
- RELEASE-CANDIDATE READY.

Không có build claim nào được phép dựa trên trạng thái local không được mô tả.

## 2. Supported baseline

Current source declares:

- platform target: Windows;
- architecture intent: x64;
- language standard: C++17;
- CMake minimum: 3.20;
- primary executable: `Im_player`.

Exact compiler/generator version và dependency versions phải được **freeze bằng evidence trong BRG-1 (#4)** trước khi baseline được coi reproducible. Policy này không tự invent phiên bản chưa được chứng minh.

## 3. Canonical build entry

Repository phải hội tụ về **một documented CMake entry path**.

Rules:

1. `CMakePresets.json` là nơi ưu tiên để expose canonical configure/build presets.
2. Không để `CMakeLists.txt`, `CMakePresets.json`, `CMakeSettings.json` định nghĩa toolchain trái ngược nhau.
3. IDE-specific settings không được là dependency bắt buộc để build.
4. Mọi command CI phải có equivalent command chạy được từ fresh clone.
5. Path tuyệt đối theo máy developer bị cấm.

BRG-1 chịu trách nhiệm chốt preset/toolchain chính thức.

## 4. Fresh-clone rule

Một BUILD PASS hợp lệ phải bắt đầu từ trạng thái có thể tái tạo tương đương:

```text
git clone
checkout exact SHA
provision documented dependencies
configure
build
run required validation
```

Không được phụ thuộc vào:
- DLL/lib bí mật nằm sẵn trên máy;
- ignored directory không có provisioning step;
- Visual Studio user cache;
- environment variable không được document;
- file copy thủ công không có version/hash/source.

## 5. Dependency authority

### vcpkg-managed dependencies

SDL2, FreeType, CPR và OpenSSL hiện được khai báo trong `vcpkg.json`.

Canonical rule:
- một dependency phải có **một authority rõ ràng** cho header + binary;
- không compile với vendored header nhưng link binary phiên bản khác mà không có explicit compatibility contract;
- dependency version/baseline phải được pin/freeze khi BRG-1 hoàn tất;
- thay dependency version phải có Issue + build/regression evidence.

### Vendored source

Vendored third-party code phải:
- có nguồn/version/license xác định;
- không bị sửa tùy tiện cùng feature PR;
- nếu fork/patch, patch rationale phải document;
- không chứa CI/example/build noise nếu không cần cho product, trừ khi intentionally retained.

### libmpv

`libmpv` là runtime/build dependency bắt buộc.

`libs/libmpv.dll.a` và `libs/libmpv-2.dll` không được tồn tại như undocumented local-only prerequisite.

BRG-1 phải chọn và document một provisioning method có:
- source/release identity;
- version;
- integrity/version evidence;
- include/import library/runtime DLL mapping;
- CI-compatible provisioning.

Cho tới khi việc này hoàn tất, fresh-clone build được xem là **BLOCKED/UNVERIFIED**, không phải PASS.

## 6. Build configurations

Required canonical configurations:

- **Debug**
- **Release**

Mỗi configuration phải:
- configure sạch;
- compile/link thành công;
- tạo executable ở documented output;
- có runtime dependency closure đủ để launch.

Nếu chỉ Debug PASS thì không được ghi "Build PASS" chung; phải ghi "Debug PASS / Release UNVERIFIED".

## 7. Source graph rule

Mọi implementation source phải thuộc một trong ba trạng thái explicit:

```text
BUILD
EXCLUDED-PROTOTYPE
REMOVED
```

Không để `.cpp/.c` active-looking nhưng vô tình không compile.

Khi thêm source mới:
- update CMake target;
- hoặc document intentional exclusion.

BRG-1 phải reconcile `native/src/api/api_manager.cpp`.

## 8. Compiler and warnings

- Build phải sử dụng compiler/toolchain đã freeze.
- New warnings do changed code tạo ra phải được xử lý hoặc explicitly justified.
- Không disable warning toàn project chỉ để merge một fix.
- Warning-as-error chỉ được bật thành required gate sau khi baseline warning inventory sạch/được freeze.

## 9. Runtime dependency closure

Sau build, launch environment phải có các DLL/resource cần thiết bằng documented step.

Tối thiểu audit:
- libmpv runtime;
- SDL2/runtime dependencies;
- vcpkg runtime DLLs;
- fonts/config/shaders cần khi startup;
- paths phụ thuộc working directory.

CI/artifact packaging không được PASS nếu executable build được nhưng không thể launch do missing runtime dependency.

## 10. Required validation levels

### L0 — Configure

- fresh configure PASS;
- dependency resolution PASS.

### L1 — Compile/Link

- Debug PASS;
- Release PASS.

### L2 — Startup/Shutdown Smoke

- app starts;
- app exits cleanly;
- no immediate missing-DLL/config/resource failure.

### L3 — Functional Smoke

Khi code path liên quan:
- Play;
- Pause/Resume;
- Seek;
- Resize;
- Close;
- Shutdown during active playback.

### L4 — Concurrency/Lifetime Stress

Bắt buộc với thay đổi liên quan:
- thread;
- callback;
- Window/Player lifetime;
- render;
- audio;
- shutdown.

Tests phải nhắm trực tiếp race/lifetime contract bị thay đổi.

### L5 — Specialized Validation

Ví dụ:
- multi-window;
- audio generation;
- render frame lifetime;
- sanitizer/Application Verifier;
- performance baseline.

Canonical Issue quyết định L5 nào required.

## 11. CI policy

Khi BRG-2 (#5) hoàn thành:

- required build workflow phải chạy trên PR;
- Debug + Release là required checks;
- CI run phải gắn exact PR head SHA;
- failed/cancelled/stale run không được coi PASS;
- rerun phải ghi attempt mới;
- artifact/log phải được giữ đủ để debug failures.

Trước BRG-2:
- local evidence được phép,
- nhưng phải ghi rõ `CI=NOT_AVAILABLE`, không được giả là CI PASS.

## 12. Test evidence format

Mỗi build/test report tối thiểu:

```text
commit=<full SHA>
configuration=<Debug|Release>
toolchain=<exact>
dependency_baseline=<exact or BRG reference>
command=<exact>
result=<PASS|FAIL>
log=<artifact/link/path>
```

Runtime test thêm:
```text
scenario=<name>
iterations=<count when stress>
observed=<result>
```

## 13. Build-affecting change rule

Các thay đổi sau bắt buộc BUILD/CI impact review:

- `CMakeLists.txt`;
- `CMakePresets.json`;
- `vcpkg.json`;
- toolchain/compiler;
- dependency headers/libs;
- runtime DLL provisioning;
- output paths;
- CI workflows;
- compile definitions;
- source list;
- platform/architecture flags.

PR phải giải thích backward compatibility và rollback.

## 14. No hidden build repair

Không được:
- copy file vào ignored directory rồi chỉ nói "build pass";
- sửa local IDE setting nhưng không commit contract;
- dùng dependency cache để che missing provisioning;
- bỏ test/source khỏi build để làm CI xanh mà không có Issue decision.

## 15. Release-candidate gate

Một commit chỉ có thể được gọi release-candidate-ready khi:

- L0 PASS;
- L1 Debug + Release PASS;
- L2 PASS;
- required L3/L4/L5 PASS;
- CI PASS nếu CI đã canonical;
- runtime dependency closure PASS;
- no unresolved P0 build/lifetime blocker;
- exact SHA recorded.

## 16. Current BRG relationship

#4 BRG-1 phải làm cho §§2–9 reproducible.

#5 BRG-2 phải canonicalize §11.

#8 BRG-5 phải cung cấp phần lớn L2–L5 baseline evidence.

#9 BRG-6 mới được dùng evidence đó để authorize Phase 0.
