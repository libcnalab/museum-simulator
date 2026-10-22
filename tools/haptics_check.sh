#!/usr/bin/env bash
# The haptics room (dev-haptics) against a force-feedback device of the kernel's own: a small Python helper creates a
# virtual evdev device through /dev/uinput that has force-feedback capabilities only (no keys, axes or pointer, so no
# desktop treats it as input), answers the kernel's effect uploads and erasures as a driver does, and records every effect,
# play, stop, gain and autocenter request. The museum (SDL's Linux haptic backend under CNA::Input::Haptics) finds it
# among /dev/input/event* and drives it; the checks compare what the museum logs with what reached the device. Only the
# motor is missing: nothing physical moves. The device exists only while the check runs.
#   tools/haptics_check.sh      exit 0 when every check passed; 77 when /dev/uinput or a prerequisite is unavailable
set -u
cd "$(dirname "$0")/.." || exit 2
bin=${MUSEUM_BIN:-${BUILD_DIR:-build}/bin/cna-3d-museum}
[ -x "$bin" ] || { echo "FAIL: $bin is not built"; exit 1; }
command -v Xvfb > /dev/null || { echo "skipped: Xvfb is not installed"; exit 77; }
[ -r /dev/uinput ] && [ -w /dev/uinput ] || { echo "skipped: /dev/uinput is not readable and writable for this user"; exit 77; }
dir=$PWD/${BUILD_DIR:-build}/haptics-check
rm -rf "$dir"
mkdir -p "$dir"
log=$dir/museum.log device=$dir/device.log name="CNA Museum haptics check (virtual)"

/usr/bin/python3 -I - "$name" > "$device" 2>&1 <<'PY' &
import ctypes, fcntl, os, select, signal, struct, sys

name = sys.argv[1].encode()
EV_FF, EV_UINPUT, UI_FF_UPLOAD, UI_FF_ERASE = 0x15, 0x0101, 1, 2
FF = {0x50: "rumble", 0x51: "periodic", 0x52: "constant", 0x53: "spring", 0x54: "friction", 0x55: "damper", 0x56: "inertia", 0x57: "ramp"}
WAVE = {0x58: "square", 0x59: "triangle", 0x5a: "sine", 0x5b: "saw-up", 0x5c: "saw-down", 0x5d: "custom"}
FF_GAIN, FF_AUTOCENTER = 0x60, 0x61
def IOC(direction, number, size): return (direction << 30) | (size << 16) | (ord("U") << 8) | number
W, R = 1, 2
UI_DEV_CREATE, UI_DEV_DESTROY = IOC(0, 1, 0), IOC(0, 2, 0)
UI_DEV_SETUP, UI_SET_EVBIT, UI_SET_FFBIT = IOC(W, 3, 92), IOC(W, 100, 4), IOC(W, 107, 4)
UI_GET_SYSNAME = IOC(R, 44, 64)
UI_BEGIN_FF_UPLOAD, UI_END_FF_UPLOAD = IOC(R | W, 200, 104), IOC(W, 201, 104)
UI_BEGIN_FF_ERASE, UI_END_FF_ERASE = IOC(R | W, 202, 12), IOC(W, 203, 12)

def say(*parts): print(*parts, flush=True)
fd = os.open("/dev/uinput", os.O_RDWR | os.O_NONBLOCK)
fcntl.ioctl(fd, UI_SET_EVBIT, EV_FF)
for bit in (0x50, 0x51, 0x52, 0x57, 0x5a, 0x58, 0x59, FF_GAIN, FF_AUTOCENTER):
    fcntl.ioctl(fd, UI_SET_FFBIT, bit)
# struct uinput_setup: input_id (bustype, vendor, product, version), name[80], ff_effects_max
fcntl.ioctl(fd, UI_DEV_SETUP, struct.pack("<HHHH80sI", 0x06, 0x1209, 0xc0de, 1, name, 16))
fcntl.ioctl(fd, UI_DEV_CREATE)
sysname = bytearray(64)
fcntl.ioctl(fd, UI_GET_SYSNAME, sysname)
sysname = sysname.split(b"\0")[0].decode()
nodes = [n for n in os.listdir(f"/sys/devices/virtual/input/{sysname}") if n.startswith("event")]
say("ready", f"/dev/input/{nodes[0]}" if nodes else "?")
running = True
def stop(*_):
    global running
    running = False
signal.signal(signal.SIGTERM, stop)
signal.signal(signal.SIGINT, stop)

def describe(effect):
    # struct ff_effect: type, id, direction, trigger(button, interval), replay(length, delay), then the union at offset 16
    kind, ident, direction, _, _, length, delay = struct.unpack_from("<HhHHHHH", effect, 0)
    text = f"{FF.get(kind, hex(kind))} direction {direction} length {length} ms delay {delay}"
    if kind == 0x52:
        level, = struct.unpack_from("<h", effect, 16)
        text += f" level {level}"
    elif kind == 0x50:
        strong, weak = struct.unpack_from("<HH", effect, 16)
        text += f" strong {strong} weak {weak}"
    elif kind == 0x51:
        waveform, period, magnitude = struct.unpack_from("<HHh", effect, 16)
        text += f" waveform {WAVE.get(waveform, hex(waveform))} period {period} magnitude {magnitude}"
    return ident, text

while running:
    ready, _, _ = select.select([fd], [], [], 0.2)
    if not ready:
        continue
    try:
        data = os.read(fd, 24 * 64)
    except BlockingIOError:
        continue
    for offset in range(0, len(data), 24):
        _, _, kind, code, value = struct.unpack_from("<qqHHi", data, offset)
        if kind == EV_UINPUT and code == UI_FF_UPLOAD:
            request = bytearray(struct.pack("<Ii", value, 0) + bytes(96))
            fcntl.ioctl(fd, UI_BEGIN_FF_UPLOAD, request)
            ident, text = describe(bytes(request[8:56]))
            struct.pack_into("<i", request, 4, 0)
            fcntl.ioctl(fd, UI_END_FF_UPLOAD, request)
            say(f"upload effect {ident}: {text}")
        elif kind == EV_UINPUT and code == UI_FF_ERASE:
            request = bytearray(struct.pack("<IiI", value, 0, 0))
            fcntl.ioctl(fd, UI_BEGIN_FF_ERASE, request)
            ident = struct.unpack_from("<I", request, 8)[0]
            fcntl.ioctl(fd, UI_END_FF_ERASE, request)
            say(f"erase effect {ident}")
        elif kind == EV_FF and code == FF_GAIN:
            say(f"gain {value}")
        elif kind == EV_FF and code == FF_AUTOCENTER:
            say(f"autocenter {value}")
        elif kind == EV_FF:
            say(f"{'play' if value else 'stop'} effect {code}" + (f" x{value}" if value > 1 else ""))
fcntl.ioctl(fd, UI_DEV_DESTROY)
os.close(fd)
say("destroyed")
PY
helper=$!
cleanup() { kill $helper 2> /dev/null; wait $helper 2> /dev/null; }
trap cleanup EXIT
for _ in $(seq 1 50); do grep -q '^ready' "$device" 2> /dev/null && break; kill -0 $helper 2> /dev/null || break; sleep 0.1; done
grep -q '^ready' "$device" || { echo "FAIL: the virtual device was not created: $(tail -2 "$device")"; exit 1; }
echo "virtual force-feedback device: $(grep '^ready' "$device" | cut -d' ' -f2)"

script="goto:dev-haptics;wait:30;press:Haptics.GetHapticsEXT;wait:5;press:Open the first device;wait:10;press:Rumble;wait:30;press:Constant effect;wait:10"
script+=";press:Gain 50;wait:10;press:Close the device;wait:10;stats;quit"
env -u WAYLAND_DISPLAY SDL_VIDEODRIVER=x11 LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=${LP_NUM_THREADS:-3} XDG_DATA_HOME="$dir/data" \
  CNA_GAMER_SERVICES_PROFILES_DIR="$dir/gamer-profiles" timeout -k 15 "${MUSEUM_TIMEOUT:-240}" \
  tools/private_xvfb.sh "$bin" --script "$script" --fixed-step 0.02 --no-audio > "$log" 2>&1
status=$?
sleep 0.5
cleanup
trap - EXIT

passed=0 failed=0
ok() { echo "PASS  $1"; passed=$((passed + 1)); }
bad() { echo "FAIL  $1"; failed=$((failed + 1)); }
line() { grep -aF -- "$1" "$log" | tail -1 | sed 's/.*haptics: //' | cut -c1-200; }
grep -aE 'haptics: [1-9][0-9]* device\(s\)' "$log" | grep -qF "$name" && ok "GetHapticsEXT lists the device: $(line 'device(s)')" || bad "device list: $(line 'device(s)')"
if ! grep -aqF "OpenEXT: open yes" "$log" && [ "$(grep -aoE "haptics: [0-9]+ device\(s\), #[0-9]+ " "$log" | sort -u | wc -l)" -gt 1 ]; then
  # A known CNA defect: every GetHapticsEXT renumbers the devices (it quits SDL's haptic subsystem when it returns), so the
  # id it lists is gone when OpenEXT looks for it. Everything that needs the open device is not verified, not passed.
  bad "OpenEXT cannot open the id GetHapticsEXT listed: the ids change on every listing ($(grep -aoE '#[0-9]+ ' "$log" | tr -d ' ' | tr '\n' ' ')), a CNA defect (docs/CNA_FINDINGS.md)"
  notrun=10
  echo "summary: $passed passed, $failed failed, $notrun not run because of it (logs: $dir)"
  python3 tools/record_profile.py haptics --passed $passed --failed $failed --not-run $notrun --note "CNA: GetHapticsEXT ids are gone by OpenEXT (subsystem quit between calls)" || true
  exit 1
fi
grep -aqF "OpenEXT: open yes, \"$name\"" "$log" && ok "OpenEXT opened it: $(line 'OpenEXT:')" || bad "OpenEXT: $(line 'OpenEXT')"
grep -aqF 'InitRumbleEXT yes, PlayRumbleEXT yes, StopRumbleEXT yes' "$log" && ok "the museum reports rumble initialised, played and stopped" || bad "rumble: $(line 'InitRumbleEXT')"
grep -qE '^upload effect [0-9]+: (rumble|periodic)' "$device" && grep -qE '^play effect' "$device" && ok "the rumble reached the device: $(grep -E '^upload effect [0-9]+: (rumble|periodic)' "$device" | head -1)" \
  || bad "no rumble upload and play at the device (device log: $device)"
grep -qE '^upload effect [0-9]+: constant .*level -?[1-9]' "$device" && ok "the constant effect reached the device: $(grep -E '^upload effect [0-9]+: constant' "$device" | head -1)" || bad "no constant effect upload at the device"
constant=$(grep -E '^upload effect [0-9]+: constant' "$device" | head -1 | grep -oE 'effect [0-9]+' | grep -oE '[0-9]+')
[ -n "$constant" ] && grep -q "^play effect $constant" "$device" && grep -q "^stop effect $constant" "$device" && grep -q "^erase effect $constant" "$device" \
  && ok "RunEffectEXT, StopEffectEXT and DestroyEffectEXT reached the device as play, stop and erase of effect $constant" || bad "constant effect $constant: play/stop/erase missing at the device"
grep -aqE 'CreateEffectEXT [0-9]+, update yes, run yes, status (yes|no), stop yes, destroyed' "$log" && ok "the museum reports the effect created, updated, run, stopped and destroyed: $(line 'CreateEffectEXT')" || bad "effect: $(line 'Effect')"
# UpdateEffectEXT raises the level from 12000 to 16000: the same effect id is uploaded again with the new level.
[ -n "$constant" ] && grep -qE "^upload effect $constant: constant .*level 12000" "$device" && grep -qE "^upload effect $constant: constant .*level 16000" "$device" \
  && ok "UpdateEffectEXT re-uploaded effect $constant with level 16000" || bad "update at the device: $(grep -E '^upload effect' "$device" | tr '\n' ';')"
# SDL scales gain 0..100 to the kernel's 0..0xFFFF.
grep -qE '^gain 3[0-9]{4}$' "$device" && ok "SetGainEXT(50) reached the device: $(grep '^gain' "$device" | tail -1)" || bad "gain at the device: $(grep '^gain' "$device" | tr '\n' ' ')"
grep -q '^autocenter 0$' "$device" && ok "SetAutocenterEXT(0) reached the device" || bad "autocenter at the device: $(grep '^autocenter' "$device" | tr '\n' ' ')"
grep -aqE 'SetGainEXT yes, SetAutocenterEXT yes, PauseEXT (yes|no), ResumeEXT (yes|no), StopAllEffectsEXT yes' "$log" && ok "the museum reports $(line 'SetGainEXT')" || bad "gain line: $(line 'SetGainEXT')"
grep -q '^destroyed$' "$device" && ok "the virtual device was removed again" || bad "the helper did not remove the device: $(tail -1 "$device")"
[ $status = 0 ] && ok "the museum exited cleanly" || bad "the museum exited with status $status (log: $log)"
grep -aqE '\[museum\]\[stats\].*exhibit errors 0 .*script errors 0' "$log" && ok "final stats with 0 exhibit and 0 script errors" || bad "stats: $(grep -a '\[stats\]\|script-error' "$log" | tail -2 | tr '\n' ' ' | cut -c1-160)"
echo "summary: $passed passed, $failed failed (logs: $dir)"
python3 tools/record_profile.py haptics --passed $passed --failed $failed || true
[ $failed = 0 ]
