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

# The microphone is a file too: 40 s of a 500 Hz tone, S16LE mono 44.1 kHz (the capture format SDL's disk driver logs).
python3 -I - "$dir/mic-input.raw" <<'PY'
import sys
import numpy as np
t = np.arange(44100 * 40) / 44100.0
open(sys.argv[1], "wb").write((np.sin(2 * np.pi * 500.0 * t) * 0.5 * 32767).astype("<i2").tobytes())
PY

# Each run starts in its room (--room): a goto to another floor rides the elevator, whose arrival chime is an arpeggio
# that would mix into the measured output.
record() {   # record NAME "ROOM;SCRIPT" [MUSEUM ARGS]: one museum run whose audio output goes to $dir/NAME.raw (format: SDL's line in the log)
  env -u WAYLAND_DISPLAY SDL_VIDEODRIVER=x11 LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=3 \
    SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE="$dir/$1.raw" SDL_AUDIO_DISK_INPUT_FILE="$dir/mic-input.raw" \
    CNA_GAMER_SERVICES_PROFILES_DIR="$dir/profiles" XDG_DATA_HOME="$dir/data" timeout -k 15 "${MUSEUM_TIMEOUT:-200}" \
    xvfb-run -a -s "-screen 0 1280x720x24" "$bin" --room "${2%%;*}" --script "${2#*;}" --fixed-step 0.02 ${3:-} > "$dir/$1.log" 2>&1
  echo $?
}

s1=$(record bell "au-generate;wait:30;press:Sound=2;wait:5;press:Play it;wait:150;stats;quit")
s2=$(record sine "au-synth;wait:30;press:Octave=4;press:Waveform=0;press:Note A;wait:120;stats;quit")
# Six chirps spread over a post-volume stretch longer than the audible start, so the analysed second half falls after the
# change and still holds chirps however slowly the build runs (a sanitizer build takes seconds over the first 20 frames).
s3=$(record silence "au-master;wait:20;press:SoundEffect.MasterVolume=0;wait:40;press:Play a test chirp;wait:40;press:Play a test chirp;wait:40;press:Play a test chirp;wait:40;press:Play a test chirp;wait:40;press:Play a test chirp;wait:40;press:Play a test chirp;wait:40;wait:40;stats;quit")
s4=$(record microphone "au-mic;wait:30;press:Start / stop capture;wait:200;press:Start / stop capture;wait:10;stats;quit")
s5=$(record panleft "au-basics;wait:30;press:Pan (left .. right)=-1;press:Play chirp;wait:80;stats;quit")
s6=$(record panright "au-basics;wait:30;press:Pan (left .. right)=1;press:Play chirp;wait:80;stats;quit")
s7=$(record xact "au-xact;wait:30;press:Cue=0;press:Play: GetCue then Play;wait:120;stats;quit")
s8=$(record xactmuted "au-xact;wait:30;press:Default category volume=0;press:Cue=0;press:Play: GetCue then Play;wait:120;stats;quit")
s9=$(record song "au-music;wait:30;press:Play song 1: arpeggio;wait:150;stats;quit")

python3 -I - "$dir" "$s1" "$s2" "$s3" "$s4" "$s5" "$s6" "$s7" "$s8" "$s9" <<'PY'
import pathlib, re, sys
import numpy as np
d = pathlib.Path(sys.argv[1])
statuses = dict(zip(("bell", "sine", "silence", "microphone", "panleft", "panright", "xact", "xactmuted", "song"), sys.argv[2:11]))
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
def load_channels(name):
    """Left and right of the mixed output (both the same for a mono device)."""
    signal, rate = load(name)
    if signal is None:
        return None, None, 0
    log = (d / f"{name}.log").read_text(errors="replace")
    channels = int(re.search(r"Writing to file \[[^\]]*\], format=\w+ channels=(\d+)", log).group(1))
    raw = (d / f"{name}.raw").read_bytes()
    data = np.frombuffer(raw[: len(raw) // (2 * channels) * 2 * channels], dtype="<i2").reshape(-1, channels).astype(np.float64) / 32768.0
    return data[:, 0], data[:, min(1, channels - 1)], rate
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
    # The room is audible from its start until MasterVolume reaches 0; how long that lasts depends on the build's speed (a
    # sanitizer build takes seconds over the first frames). The measured stretch starts after the first half second of silence
    # that follows the audible start, runs to the end and must be long enough to hold the six chirps played after the change.
    loud = np.abs(silence) >= 1e-4
    window = rate // 2
    start = int(np.argmax(loud)) if loud.any() else 0
    quiet = np.convolve(loud[start:].astype(np.int32), np.ones(window, dtype=np.int32), mode="valid") == 0
    begin = start + int(np.argmax(quiet)) if quiet.any() else len(silence)
    tail = silence[begin:]
    loudest = float(np.max(np.abs(tail))) if len(tail) else 1.0
    report(len(tail) >= 4 * rate and loudest < 1e-4,
           f"silence: with MasterVolume 0 the output is silent, chirp included (peak {loudest:.2e} over the last {len(tail) / rate:.1f} s, from {begin / rate:.1f} s)")
# Pan -1 puts the chirp in the left channel, +1 in the right (XNA's SoundEffect.Play pan). The ratio is taken over the
# loudest half second.
for name, louder, quieter in (("panleft", 0, 1), ("panright", 1, 0)):
    left, right, rate = load_channels(name)
    if left is None:
        report(False, f"{name}: no audio output"); continue
    both = np.abs(left) + np.abs(right)
    block = rate // 10
    blocks = len(both) // block
    start = int(np.argmax([both[i * block:(i + 1) * block].sum() for i in range(blocks)])) * block if blocks else 0
    energy = [float(np.sqrt(np.mean(c[start:start + rate // 2] ** 2))) for c in (left, right)]
    ratio = energy[louder] / max(energy[quieter], 1e-9)
    side = ("left", "right")[louder]
    report(energy[louder] > 0.02 and ratio > 10.0, f"{name}: the chirp is in the {side} channel (RMS left {energy[0]:.3f}, right {energy[1]:.3f}, ratio {ratio:.0f})")
# XACT: the Tone cue is the 220 Hz tone of tools/gen_assets.py; with its category (Default) at volume 0 it stays quiet.
signal, rate = load("xact")
frequency, rms = peak(signal, rate) if signal is not None else (None, 0.0)
report(frequency is not None and abs(frequency - 220.0) <= 3.0, f"xact: Cue \"Tone\" plays its 220 Hz wave ({frequency if frequency is not None else 0:.1f} Hz, RMS {rms:.3f})")
muted, rate = load("xactmuted")
_, muted_rms = peak(muted, rate) if muted is not None else (None, 1.0)
report(muted is not None and muted_rms < 0.25 * max(rms, 1e-9), f"xactmuted: with the Default category at volume 0 the cue is not heard (loudest RMS {muted_rms:.3f} against {rms:.3f})")
# MediaPlayer: song 1 is the arpeggio (C4 E4 G4 C5 G4 E4 with an octave partial); the player reports its state changes.
signal, rate = load("song")
frequency, rms = peak(signal, rate) if signal is not None else (None, 0.0)
notes = [261.63, 329.63, 392.0, 523.25]
near = frequency is not None and any(abs(frequency - f) <= 4.0 or abs(frequency - 2 * f) <= 6.0 for f in notes)
report(near, f"song: MediaPlayer plays the arpeggio (dominant {frequency if frequency is not None else 0:.1f} Hz, RMS {rms:.3f})")
song_log = (d / "song.log").read_text(errors="replace")
report("MediaPlayer.MediaStateChanged" in song_log and "MediaPlayer.ActiveSongChanged" in song_log, "song: MediaStateChanged and ActiveSongChanged were raised")
# The microphone room captured the 500 Hz file through CNA's Microphone (BufferReady, GetData) and reports it on stop.
mic = re.search(r"\[museum\]\[audio\] microphone: stopped after (\d+) BufferReady event\(s\), (\d+) bytes of 16-bit mono at (\d+) Hz, RMS ([0-9.]+), "
                r"zero-crossing frequency ([0-9.]+) Hz", (d / "microphone.log").read_text(errors="replace"))
if mic is None:
    report(False, "microphone: the room reported no capture")
else:
    events, size, rate, rms, frequency = int(mic.group(1)), int(mic.group(2)), int(mic.group(3)), float(mic.group(4)), float(mic.group(5))
    report(events >= 1 and size > 0, f"microphone: {events} BufferReady event(s), {size} bytes at {rate} Hz arrived")
    report(abs(frequency - 500.0) <= 5.0 and rms > 0.2, f"microphone: the captured signal is the 500 Hz test tone ({frequency:.1f} Hz, RMS {rms:.3f}; the file has RMS 0.354)")
print(f"summary: {passed} passed, {failed} failed (raw output and logs: {d})")
import subprocess
subprocess.run([sys.executable, "tools/record_profile.py", "audio", "--passed", str(passed), "--failed", str(failed)], check=False)
sys.exit(1 if failed else 0)
PY
