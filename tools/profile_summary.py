#!/usr/bin/env python3
"""One table of every test profile: passed, failed, skipped, unavailable and not verified, and whether the result
belongs to the binary that exists now.

    tools/profile_summary.py            exit 0 only when every profile is current and nothing failed

Each runner records its own result (tools/record_profile.py) in <build dir>/profiles/. A profile is "current" when its
museum and libcna hashes match the binaries in its build directory now; an older result is "stale", a missing one
"not run". Stale and not-run profiles count as not verified: a partly green battery never reads as complete.
"""

from __future__ import annotations

import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from record_profile import ROOT, sha256  # noqa: E402

# Every profile the museum has, with the command that produces it. Build directory per profile: build/ unless named.
PROFILES = [
    ("check_all", "build", "tools/check_all.sh  (build, tests, docs, audit, tour, nav, renderer, reference, memory)"),
    ("check_all-slow", "build", "SLOW=1 tools/check_all.sh  (adds OPENGL33 and SOFTWARE tours and memory)"),
    ("scenarios", "build", "tools/scenarios.py"),
    ("renderer-OPENGLES3", "build", "tools/renderer_check.sh OPENGLES3"),
    ("renderer-OPENGL33", "build", "tools/renderer_check.sh OPENGL33"),
    ("renderer-SOFTWARE", "build", "tools/renderer_check.sh SOFTWARE"),
    ("renderer-OPENGLES3-gpu", "build", "MUSEUM_GPU=1 tools/renderer_check.sh OPENGLES3"),
    ("renderer-OPENGL33-gpu", "build", "MUSEUM_GPU=1 tools/renderer_check.sh OPENGL33"),
    ("renderer-FNA3D", "build-probe", "BUILD_DIR=build-probe tools/renderer_check.sh FNA3D"),
    ("renderer-WEBGPU", "build-probe", "BUILD_DIR=build-probe VK_ICD_FILENAMES=.../lvp_icd.json tools/renderer_check.sh WEBGPU"),
    ("renderer-SDL_GPU", "build-probe", "BUILD_DIR=build-probe VK_ICD_FILENAMES=.../lvp_icd.json tools/renderer_check.sh SDL_GPU"),
    ("renderer-SDL_GPU-gpu", "build-probe", "MUSEUM_GPU=1 BUILD_DIR=build-probe VK_ICD_FILENAMES=.../radeon_icd.json tools/renderer_check.sh SDL_GPU"),
    ("renderer-FNA3D-gpu", "build-probe", "MUSEUM_GPU=1 BUILD_DIR=build-probe tools/renderer_check.sh FNA3D"),
    ("renderer-WEBGPU-gpu", "build-probe", "MUSEUM_GPU=weston BUILD_DIR=build-probe VK_ICD_FILENAMES=.../radeon_icd.json tools/renderer_check.sh WEBGPU"),
    ("renderer-HEADLESS", "build-probe", "BUILD_DIR=build-probe tools/renderer_check.sh HEADLESS  (no readback: one SKIP per room)"),
    ("devices", "build", "tools/devices_check.sh"),
    ("audio", "build", "tools/audio_check.sh"),
    ("voice", "build", "tools/voice_check.sh"),
    ("net", "build", "tools/net_check.sh"),
    ("online", "build", "tools/online_check.sh"),
    ("online-party", "build", "tools/online_party_check.sh  (three accounts: invitation, private slot, party, sign-out)"),
    ("window", "build", "tools/window_check.sh  (drops, focus and minimise with openbox on a private display)"),
    ("tray", "build", "tools/tray_check.sh  (the tray icon against a private StatusNotifier host)"),
    ("web-webgl2", "build-consumer", "tools/web_smoke.sh"),
    ("web-webgl2-tour", "build-consumer", "tools/web_tour.sh  (every room and control in headless Chrome)"),
    ("web-webgpu-tour", "build-consumer", "MUSEUM_WEB_RENDERER=WEBGPU tools/web_tour.sh  (browser WebGPU on the GPU)"),
    ("web-webgl2-scenarios", "build-consumer", "tools/web_scenarios.py  (the functional scenarios in the page)"),
    ("android-tour", "build-consumer", "MUSEUM_ANDROID_SKIP=model-sampler tools/android_run.sh  (x86_64 emulator, tools/build_android.sh first)"),
    ("wine-software-tour", "build-consumer/windows", "tools/wine_tour.sh -p  (MinGW build under Wine, SOFTWARE)"),
    ("wine-directx9-tour", "build-consumer/windows", "CNA_GRAPHICS_RENDERER=DIRECTX9 tools/wine_tour.sh -p  (Wine's Direct3D 9)"),
]


def main() -> int:
    rows = []
    totals = {"current": 0, "stale": 0, "not run": 0, "failing": 0}
    for name, build_dir, command in PROFILES:
        path = ROOT / build_dir / "profiles" / f"{name}.json"
        if not path.is_file():
            rows.append((name, "not run", "-", "-", "-", "-", "-", command))
            totals["not run"] += 1
            continue
        record = json.loads(path.read_text(encoding="utf-8"))
        binary = ROOT / record.get("binary", f"{build_dir}/bin/cna-3d-museum")
        library = ROOT / record.get("library", f"{build_dir}/cna/libcna.so")
        # A result whose program no longer exists cannot be current (a missing file hashes to None on both sides).
        current = record.get("museumBinarySha256") is not None and record.get("museumBinarySha256") == sha256(binary) \
            and record.get("cnaBinarySha256") == sha256(library)
        state = "current" if current else "stale"
        totals[state] += 1
        if record["failed"]:
            totals["failing"] += 1
        rows.append((name, state + ("" if current else f" ({record['recordedAt'][:16]})"), str(record["passed"]), str(record["failed"]),
                     str(record["skipped"]), str(record["unavailable"]), str(record["notRun"]), record.get("note") or command))
    header = ("profile", "result", "pass", "fail", "skip", "unavail.", "not run", "note / command")
    widths = [max(len(row[i]) for row in rows + [header]) for i in range(7)]
    print("  ".join(header[i].ljust(widths[i]) for i in range(7)) + "  " + header[7])
    for row in rows:
        print("  ".join(row[i].ljust(widths[i]) for i in range(7)) + "  " + row[7])
    complete = totals["stale"] == 0 and totals["not run"] == 0 and totals["failing"] == 0
    print()
    if complete:
        print(f"summary: complete: all {len(PROFILES)} profiles are current and nothing failed")
    else:
        print(f"summary: NOT complete: {totals['current']} current, {totals['stale']} stale, {totals['not run']} not run, "
              f"{totals['failing']} with failures (stale and not-run profiles are not verified)")
    return 0 if complete else 1


if __name__ == "__main__":
    sys.exit(main())
