"""Issue #64 — compile and execute the real BadgeFmt body, with only GUI drawing stubbed.

The hosted MSVC probe must compile with /WX: C5082 is an actual ABI failure.
No media player, ImGui window, or audio source is launched.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "native/src/gui/widgets/gui_status.cpp"


def function(source: str, signature: str) -> str:
    match = re.search(r"\bvoid\s+" + re.escape(signature) + r"\s*\(", source)
    if not match:
        raise AssertionError(f"missing function: {signature}")
    brace = source.index("{", match.end())
    depth = 0
    for i in range(brace, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():i + 1]
    raise AssertionError(f"unclosed function: {signature}")


class BadgeFmtSourceContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = SOURCE.read_text(encoding="utf-8")
        cls.badge = function(src, "BadgeFmt")
        cls.text = function(src, "TextStatusFmt")

    def test_last_named_parameter_is_flags(self):
        signature = self.badge.split("{", 1)[0]
        self.assertRegex(signature, r"BadgeFlags\s+flags\s*,\s*\.\.\.\s*\)")

    def test_variadic_start_uses_last_named_parameter(self):
        self.assertRegex(self.badge, r"va_start\s*\(\s*args\s*,\s*flags\s*\)")
        self.assertNotRegex(self.badge, r"va_start\s*\(\s*args\s*,\s*fmt\s*\)")

    def test_va_end_and_bounded_formatter_remain(self):
        self.assertEqual(len(re.findall(r"\bva_start\s*\(", self.badge)), 1)
        self.assertEqual(len(re.findall(r"\bva_end\s*\(\s*args\s*\)", self.badge)), 1)
        self.assertIn("vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt, args)", self.badge)
        self.assertIn("vsnprintf(buffer, sizeof(buffer), fmt, args)", self.badge)
        self.assertIn("Badge(buffer, type, flags)", self.badge)
        self.assertRegex(self.badge, r"if\s*\(!fmt\)")

    def test_unrelated_textstatusfmt_control(self):
        self.assertRegex(self.text, r"va_start\s*\(\s*args\s*,\s*fmt\s*\)")


CPP_PREFIX = r"""
#include <cstdarg>
#include <cstdio>
#include <string>
#include <iostream>
enum class StatusType { None, Success, Info };
using BadgeFlags = int;
namespace CSImGui {
static std::string captured;
static int captured_flags = -1;
static StatusType captured_type = StatusType::None;
static int calls = 0;
void Badge(const char* text, StatusType type, BadgeFlags flags) {
    captured = text ? text : "<null>";
    captured_type = type;
    captured_flags = flags;
    ++calls;
}
"""
CPP_SUFFIX = r"""
} // CSImGui
int main() {
    using namespace CSImGui;
    BadgeFmt("value=%d word=%s", StatusType::Success, 17, 42, "ok");
    if (captured != "value=42 word=ok" || captured_flags != 17 ||
        captured_type != StatusType::Success || calls != 1) return 11;

    BadgeFmt("", StatusType::Info, 3);
    if (captured != "" || captured_flags != 3 || calls != 2) return 12;

    BadgeFmt("negative=%d zero=%d", StatusType::Info, 1, -7, 0);
    if (captured != "negative=-7 zero=0" || calls != 3) return 13;

    const std::string long_text(1600, 'x');
    BadgeFmt("%s", StatusType::Info, 2, long_text.c_str());
    if (captured != long_text.substr(0, 1023) || calls != 4) return 14;

    BadgeFmt(nullptr, StatusType::Info, 9);
    if (calls != 4) return 15;

    std::cout << "ISSUE64_BADGEFMT_ABI_RUNTIME_PASS\n";
    return 0;
}
"""


def emit_msvc(path: Path):
    path.mkdir(parents=True, exist_ok=True)
    real_function = function(SOURCE.read_text(encoding="utf-8"), "BadgeFmt")
    (path / "main.cpp").write_text(
        CPP_PREFIX + "\n" + real_function + "\n" + CPP_SUFFIX,
        encoding="utf-8",
    )
    (path / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.25)\n"
        "project(issue64_badgefmt_abi LANGUAGES CXX)\n"
        "add_executable(issue64_probe main.cpp)\n"
        "target_compile_features(issue64_probe PRIVATE cxx_std_17)\n"
        "if(MSVC)\n"
        "  target_compile_options(issue64_probe PRIVATE /W4 /WX /we5082)\n"
        "endif()\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--emit-msvc", type=Path)
    args = parser.parse_args()
    if args.emit_msvc:
        emit_msvc(args.emit_msvc)
    else:
        unittest.main(argv=["test_issue64_badgefmt_abi.py"])
