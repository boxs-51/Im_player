"""Issue #63: frozen NotifyMPV state-ownership contract and isolated MSVC test.

The Windows executable extracts the actual production function and reads the
repository's real PlayerStateSystem model; only the WinToast side effect is
replaced by an in-memory capture. It does NOT prove GUI/toast delivery.
"""
import argparse
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "native/src/notification.cpp"
HEADER = ROOT / "native/src/notification.h"


def get_function(src):
    match = re.search(r"\bvoid\s+NotifyMPV\s*\(", src)
    if match is None:
        raise AssertionError("NotifyMPV missing")
    brace = src.find("{", match.end())
    if brace < 0:
        raise AssertionError("NotifyMPV body missing")
    depth = 0
    for pos in range(brace, len(src)):
        if src[pos] == "{":
            depth += 1
        elif src[pos] == "}":
            depth -= 1
            if depth == 0:
                return src[match.start():pos + 1]
    raise AssertionError("unterminated NotifyMPV")


class NotificationStateSourceOracle(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.body = get_function(SOURCE.read_text(encoding="utf-8"))
        cls.header = HEADER.read_text(encoding="utf-8")

    def test_s01_explicit_authoritative_model_argument(self):
        self.assertRegex(self.body, r"NotifyMPV\s*\(\s*const\s+PlayerStateSystem\s*&\s*state\s*\)")
        self.assertRegex(self.header, r"NotifyMPV\s*\(\s*const\s+PlayerStateSystem\s*&\s*state\s*\)")

    def test_s02_reads_single_locked_playback_snapshot(self):
        self.assertRegex(self.body, r"state\s*\.\s*ReadPlayback\s*\(")
        self.assertRegex(self.body, r"PlaybackModel")
        self.assertNotRegex(self.body, r"\bbool\s+paused\s*;")

    def test_s03_only_consistent_playing_paused_states(self):
        self.assertIn("PlaybackState::Playing", self.body)
        self.assertIn("PlaybackState::Paused", self.body)
        self.assertIn("flags.isPaused", self.body)
        self.assertIn("ShowNotification", self.body)

    def test_control_transport_and_named_pipe_untouched(self):
        src = SOURCE.read_text(encoding="utf-8")
        self.assertIn("void ShowNotification(const std::wstring& title, const std::wstring& content)", src)
        self.assertIn("void PipeServerThread()", src)
        self.assertIn("void SendArgsToFirstInstance(int argc, char* argv[])", src)


CPP_PREFIX = r"""
#include "player/PlayerStateSystem.h"
#include <optional>
#include <string>
#include <iostream>
#include <thread>
namespace {
std::wstring last_title;
std::wstring last_status;
int toast_count = 0;
}
void ShowNotification(const std::wstring& title, const std::wstring& text) {
    last_title = title;
    last_status = text;
    ++toast_count;
}
"""
CPP_SUFFIX = r"""
static void set_model(PlayerStateSystem& model, PlaybackState state, bool pause) {
    model.WritePlayback([&](PlaybackModel& m) {
        m.state = state;
        m.flags.isPaused = pause;
    });
}
int main() {
    PlayerStateSystem player;
    // Unknown/default state cannot be promoted to "Playing" or "Paused".
    NotifyMPV(player);
    if (toast_count != 0) return 11;

    set_model(player, PlaybackState::Playing, false);
    NotifyMPV(player);
    if (toast_count != 1 || last_title != L"MPV" || last_status != L"Playing") return 12;

    set_model(player, PlaybackState::Paused, true);
    NotifyMPV(player);
    if (toast_count != 2 || last_status != L"Paused") return 13;

    // A mismatch between derived state and observed mpv pause is not valid proof.
    set_model(player, PlaybackState::Playing, true);
    NotifyMPV(player);
    set_model(player, PlaybackState::Paused, false);
    NotifyMPV(player);
    if (toast_count != 2) return 14;

    const PlaybackState invalid[] = {PlaybackState::Idle, PlaybackState::Loading,
        PlaybackState::Seeking, PlaybackState::EndOfFile, PlaybackState::NoFile};
    for (auto value : invalid) {
        for (bool paused : {false, true}) {
            set_model(player, value, paused);
            NotifyMPV(player);
        }
    }
    if (toast_count != 2) return 15;

    // Same state owner, changed asynchronously while read uses shared mutex.
    std::thread writer([&] {
        for (int i = 0; i < 200; ++i) {
            if (i % 2 == 0) set_model(player, PlaybackState::Playing, false);
            else set_model(player, PlaybackState::Paused, true);
        }
    });
    for (int i = 0; i < 200; ++i) NotifyMPV(player);
    writer.join();
    if (toast_count < 2) return 16;
    if (last_status != L"Playing" && last_status != L"Paused") return 17;

    std::cout << "ISSUE63_NOTIFY_STATE_RUNTIME_PASS\n";
    return 0;
}
"""


def emit_msvc(path: Path):
    path.mkdir(parents=True, exist_ok=True)
    f = get_function(SOURCE.read_text(encoding="utf-8"))
    (path / "main.cpp").write_text(CPP_PREFIX + "\n" + f + "\n" + CPP_SUFFIX, encoding="utf-8")
    include_dir = (ROOT / "native/src").as_posix()
    (path / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.25)\n"
        "project(issue63_notify_model LANGUAGES CXX)\n"
        "add_executable(issue63_probe main.cpp)\n"
        "target_compile_features(issue63_probe PRIVATE cxx_std_17)\n"
        f'target_include_directories(issue63_probe PRIVATE "{include_dir}")\n'
        "if(MSVC)\n"
        "  target_compile_options(issue63_probe PRIVATE /W4 /WX /we4700)\n"
        "endif()\n",
        encoding="utf-8")


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--emit-msvc", type=Path)
    a = p.parse_args()
    if a.emit_msvc:
        emit_msvc(a.emit_msvc)
    else:
        unittest.main(argv=["test_issue63_notification_state.py"])
