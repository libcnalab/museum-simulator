#!/usr/bin/env bash
# Network voice between two museum processes (backlog N05), with no microphone and no loudspeaker: SDL's "disk" audio
# driver reads the talker's microphone from a raw file (a generated 300 Hz tone) and writes the listener's mixed output
# to another. A hosts a SystemLink session, B joins; CNA routes the voice itself (Opus, 16 kHz). B must log that A's
# gamer started talking, and B's output must contain the tone. Neither process has a real audio device.
#
#   tools/voice_check.sh        exit 0 when every check passed; needs xvfb-run and python3 with numpy
set -u
cd "$(dirname "$0")/.." || exit 2
command -v xvfb-run > /dev/null || { echo "skipped: xvfb-run is not installed"; exit 77; }
python3 -c "import numpy" 2> /dev/null || { echo "skipped: python3 numpy is not installed"; exit 77; }
bin=$PWD/${MUSEUM_BIN:-${BUILD_DIR:-build}/bin/cna-3d-museum}
[ -x "$bin" ] || { echo "FAIL: $bin is not built"; exit 1; }
dir=$PWD/${BUILD_DIR:-build}/voice-check
rm -rf "$dir"
mkdir -p "$dir"
mode=$((1 + RANDOM % 9))
# 40 s of a 300 Hz tone, S16LE mono 44.1 kHz: the format SDL's disk driver opens the capture device with (it logs it).
python3 -I - "$dir/microphone.raw" <<'PY'
import sys
import numpy as np
t = np.arange(44100 * 40) / 44100.0
tone = (np.sin(2 * np.pi * 300.0 * t) * 0.5 * 32767).astype("<i2")
open(sys.argv[1], "wb").write(tone.tobytes())
PY
pids=()
cleanup() { for p in "${pids[@]}"; do kill "$p" 2> /dev/null; done; }
trap cleanup EXIT
launch() {   # launch NAME SCRIPT [extra env...]
  local name=$1 script=$2; shift 2
  mkdir -p "$dir/$name-profiles"
  env -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS SDL_VIDEODRIVER=x11 LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 \
    SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE="$dir/$name-output.raw" SDL_AUDIO_DISK_INPUT_FILE="$dir/microphone.raw" "$@" \
    CNA_GAMER_SERVICES_PROFILES_DIR="$dir/$name-profiles" XDG_DATA_HOME="$dir/$name-data" timeout -k 15 "${MUSEUM_TIMEOUT:-240}" \
    xvfb-run -a -s "-screen 0 1280x720x24" "$bin" --script "$script" --fixed-step 0.02 > "$dir/$name.log" 2>&1 &
  pids+=($!)
}
launch talker "goto:net-session;wait:20;press:Deliver packets=1;wait:20;press:SessionProperties[0] (game mode)=$mode;wait:1600;stats;quit"
sleep 4
launch listener "goto:net-session;wait:120;press:SessionProperties[0] (game mode)=$mode;press:Search for game mode=1;press:NetworkSession.BeginFind;wait:250;press:NetworkSession.BeginJoin;wait:1200;stats;quit" SDL_AUDIO_DISK_INPUT_FILE=/dev/null
for p in "${pids[@]}"; do wait "$p"; done
pids=()

python3 -I - "$dir" <<'PY'
import pathlib, re, sys
import numpy as np
d = pathlib.Path(sys.argv[1]); passed = failed = 0
def report(ok, text):
    global passed, failed
    print(("PASS  " if ok else "FAIL  ") + text); passed += ok; failed += not ok
logs = {name: (d / f"{name}.log").read_text(errors="replace") for name in ("talker", "listener")}
for name, log in logs.items():
    report(re.search(r"\[museum\]\[stats\].* exhibit errors 0 .*script errors 0", log) is not None, f"{name}: ran with audio on, no exhibit or script errors")
report(re.search(r"\[museum\]\[net\] Joined: ", logs["listener"]) is not None, "the listener joined the talker's session")
talking = re.search(r"\[museum\]\[net\] voice: (\S+) started talking \(HasVoice (true|false)\)", logs["listener"])
report(talking is not None, "the listener saw the talker start talking" + (f": {talking.group(1)}, HasVoice {talking.group(2)}" if talking else ""))
spec = re.search(r"Writing to file \[[^\]]*listener-output\.raw\], format=(\w+) channels=(\d+) freq=(\d+)", logs["listener"])
raw = (d / "listener-output.raw").read_bytes() if (d / "listener-output.raw").is_file() else b""
if spec is None or not raw:
    report(False, "the listener wrote no audio output")
else:
    channels, rate = int(spec.group(2)), int(spec.group(3))
    data = np.frombuffer(raw[: len(raw) // (2 * channels) * 2 * channels], dtype="<i2").reshape(-1, channels).mean(axis=1) / 32768.0
    block = rate // 10
    blocks = len(data) // block
    energy = np.sqrt(np.mean(data[: blocks * block].reshape(blocks, block) ** 2, axis=1)) if blocks else np.array([0.0])
    loud = int(np.argmax(energy))
    window = data[loud * block : loud * block + rate // 2]
    if energy[loud] < 0.01 or len(window) < block:
        report(False, f"the listener's output stayed silent (loudest 0.1 s RMS {float(energy[loud]):.4f})")
    else:
        spectrum = np.abs(np.fft.rfft(window * np.hanning(len(window))))
        peak = float(np.fft.rfftfreq(len(window), 1.0 / rate)[int(np.argmax(spectrum))])
        report(abs(peak - 300.0) <= 6.0, f"the talker's 300 Hz tone came out of the listener: dominant {peak:.1f} Hz, RMS {float(energy[loud]):.3f}")
print(f"summary: {passed} passed, {failed} failed (logs and audio: {d})")
import subprocess
subprocess.run([sys.executable, "tools/record_profile.py", "voice", "--passed", str(passed), "--failed", str(failed)], check=False)
sys.exit(1 if failed else 0)
PY
