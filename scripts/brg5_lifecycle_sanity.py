from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]

required = {
    "native/src/windows/WindowController.cpp": [
        '"Window"', '"CREATE"', '"START"', '"STOP"', '"DESTROY"'
    ],
    "native/src/player/session/PlayerSession.cpp": [
        '"PlayerSession"', '"CREATE"', '"START"', '"STOP"', '"DESTROY"'
    ],
    "native/src/player/render/PlayBackRender.cpp": [
        '"MPVRenderContext"', '"CREATE"', '"START"', '"STOP"', '"DESTROY"'
    ],
    "native/src/windows/UIRenderThread.cpp": [
        '"UIRenderThread"', '"CREATE"', '"START"', '"STOP"', '"JOIN"', '"DESTROY"'
    ],
    "native/src/player/render/PlayBackRenderThread.cpp": [
        '"PlayBackRenderThread"', '"CREATE"', '"START"', '"STOP"', '"JOIN"', '"DESTROY"'
    ],
    "native/src/player/audio/AudioCaptureManager.cpp": [
        '"AudioCaptureManager"', '"CREATE"', '"START"', '"STOP"', '"JOIN"', '"DESTROY"'
    ],
    "native/src/player/audio/AudioProcessor.cpp": [
        '"AudioProcessor"', '"CREATE"', '"START"', '"STOP"', '"JOIN"', '"DESTROY"'
    ],
    "native/src/player/audio/AudioOutputWorker.cpp": [
        '"AudioOutputWorker"', '"CREATE"', '"START"', '"STOP"', '"JOIN"', '"DESTROY"'
    ],
}

errors = []
for rel, markers in required.items():
    path = ROOT / rel
    if not path.is_file():
        errors.append(f"missing file: {rel}")
        continue
    text = path.read_text(encoding="utf-8")
    if "LifecycleEvidence::Emit" not in text:
        errors.append(f"{rel}: no LifecycleEvidence::Emit calls")
    for marker in markers:
        if marker not in text:
            errors.append(f"{rel}: missing marker token {marker}")

helper = ROOT / "native/src/common/LifecycleEvidence.h"
if not helper.is_file():
    errors.append("missing lifecycle evidence helper")
else:
    helper_text = helper.read_text(encoding="utf-8")
    for token in (
        "IM_PLAYER_LIFECYCLE_LOG",
        "OutputDebugStringA",
        "GetCurrentProcessId",
        "GetCurrentThreadId",
    ):
        if token not in helper_text:
            errors.append(f"helper missing token: {token}")

if errors:
    for error in errors:
        print(f"[BRG5-A] ERROR: {error}", file=sys.stderr)
    raise SystemExit(1)

print("[BRG5-A] lifecycle marker coverage PASS")
