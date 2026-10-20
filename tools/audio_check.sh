#!/usr/bin/env bash
# The audio output path with the audio system switched on (backlog T06). Tours run with --no-audio; this check runs
# the museum with SDL's "disk" audio driver, which writes the exact mixed output the device receives to a raw file,
# and then measures it: a generated bell must come out with its 880 Hz fundamental, the synthesiser's A4 sine at
# 440 Hz, and with SoundEffect.MasterVolume at 0 the output must fall silent (rooms play ambience otherwise). This proves CNA mixed and delivered the samples; it is not
# a listening test (whether a loudspeaker makes the sound audible is a manual check).
#
#   tools/audio_check.sh        exit 0 when every check passed; needs xvfb-run and python3 with numpy
set -u
cd "$(dirname "$0")/.." || exit 2
command -v xvfb-run > /dev/null || { echo "skipped: xvfb-run is not installed"; exit 77; }
python3 -c "import numpy" 2> /dev/null || { echo "skipped: python3 numpy is not installed"; exit 77; }
bin=$PWD/${MUSEUM_BIN:-${BUILD_DIR:-build}/bin/cna-3d-museum}
[ -x "$bin" ] || { echo "FAIL: $bin is not built"; exit 1; }
dir=$PWD/${BUILD_DIR:-build}/audio-check
rm -rf "$dir"
mkdir -p "$dir"

record() {   # record NAME SCRIPT: one museum run whose audio output goes to $dir/NAME.raw (format: SDL's line in the log)
  env -u WAYLAND_DISPLAY SDL_VIDEODRIVER=x11 LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=3 \
    SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE="$dir/$1.raw" \
    CNA_GAMER_SERVICES_PROFILES_DIR="$dir/profiles" XDG_DATA_HOME="$dir/data" timeout -k 15 "${MUSEUM_TIMEOUT:-200}" \
    xvfb-run -a -s "-screen 0 1280x720x24" "$bin" --script "$2" --fixed-step 0.02 > "$dir/$1.log" 2>&1
  echo $?
}

s1=$(record bell "goto:au-generate;wait:30;press:Sound=2;wait:5;press:Play it;wait:150;stats;quit")
s2=$(record sine "goto:au-synth;wait:30;press:Octave=4;press:Waveform=0;press:Note A;wait:120;stats;quit")
s3=$(record silence "goto:au-master;wait:20;press:SoundEffect.MasterVolume=0;wait:10;press:Play a test chirp;wait:150;stats;quit")

python3 -I - "$dir" "$s1" "$s2" "$s3" <<'PY'
import pathlib, re, sys
import numpy as np
d = pathlib.Path(sys.argv[1]); statuses = dict(zip(("bell", "sine", "silence"), sys.argv[2:5]))
passed = failed = 0
def report(ok, text):
    global passed, failed
    print(("PASS  " if ok else "FAIL  ") + text)
    passed += ok; failed += not ok
def load(name):
    """The mixed output as mono floats, decoded in the format SDL's disk driver reports in the log."""
    path = d / f"{name}.raw"
    log = (d / f"{name}.log").read_text(errors="replace")
    spec = re.search(r"format=(\w+) channels=(\d+) freq=(\d+)", log)
    if not path.is_file() or spec is None:
        return None, 0
    kind, channels, rate = spec.group(1), int(spec.group(2)), int(spec.group(3))
    dtype = {"S16LE": "<i2", "S16": "<i2", "F32LE": "<f4", "F32": "<f4", "S32LE": "<i4"}.get(kind)
    if dtype is None:
        return None, 0
    raw = path.read_bytes()
    width = np.dtype(dtype).itemsize * channels
    data = np.frombuffer(raw[: len(raw) // width * width], dtype=dtype).reshape(-1, channels).astype(np.float64)
    if dtype == "<i2":
        data /= 32768.0
    elif dtype == "<i4":
        data /= 2147483648.0
    return data.mean(axis=1), rate
for name, status in statuses.items():
    log = (d / f"{name}.log").read_text(errors="replace")
    clean = status == "0" and re.search(r"\[museum\]\[stats\].* exhibit errors 0 .*script errors 0", log) is not None
    report(clean, f"{name}: the museum ran with audio on and exited cleanly (status {status})")
def peak(signal, rate):
    # The rooms play quiet ambience; the sound under test is the loudest part, so the analysis starts at the loudest
    # tenth of a second and covers the half second after it.
    block = rate // 10
    blocks = len(signal) // block
    if blocks < 2:
        return None, 0.0
    energy = np.sqrt(np.mean(signal[: blocks * block].reshape(blocks, block) ** 2, axis=1))
    loudest = int(np.argmax(energy))
    if energy[loudest] < 0.02:
        return None, float(energy[loudest])
    window = signal[loudest * block : loudest * block + rate // 2]
    spectrum = np.abs(np.fft.rfft(window * np.hanning(len(window))))
    frequencies = np.fft.rfftfreq(len(window), 1.0 / rate)
    return float(frequencies[int(np.argmax(spectrum))]), float(np.sqrt(np.mean(window ** 2)))
for name, expected in (("bell", 880.0), ("sine", 440.0)):
    signal, rate = load(name)
    if signal is None:
        report(False, f"{name}: no audio output (or no format line) was written"); continue
    frequency, rms = peak(signal, rate)
    if frequency is None:
        report(False, f"{name}: the output stayed silent ({len(signal) / rate:.1f} s written)"); continue
    report(abs(frequency - expected) <= 3.0, f"{name}: dominant frequency {frequency:.1f} Hz (expected {expected:.0f} Hz), RMS {rms:.3f}, {rate} Hz output")
silence, rate = load("silence")
if silence is None:
    report(False, "silence: no audio output was written")
else:
    tail = silence[len(silence) // 2 :]   # well after MasterVolume was set to 0 and the chirp was played
    loudest = float(np.max(np.abs(tail))) if len(tail) else 1.0
    report(loudest < 1e-4, f"silence: with MasterVolume 0 the output is silent, chirp included (peak {loudest:.2e} over the last {len(tail) / rate:.1f} s)")
print(f"summary: {passed} passed, {failed} failed (raw output and logs: {d})")
sys.exit(1 if failed else 0)
PY
