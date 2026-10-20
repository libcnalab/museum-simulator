#!/usr/bin/env bash
# The Android edition on a headless x86_64 emulator: every room visited (or the ones named), the renderer self-check rooms
# running their checks, as tools/web_tour.sh does in a browser. Needs tools/build_android.sh first.
#
#   tools/android_run.sh [room ...]                  tour; MUSEUM_ANDROID_PRESSALL=1 also operates every control
#   MUSEUM_ANDROID_SCRIPT="a;b;c" tools/android_run.sh  one script instead of the tour (expected lines: MUSEUM_ANDROID_EXPECT)
#
# The emulator runs the shared AVD read-only (MUSEUM_ANDROID_AVD, default Medium_Phone; nothing in it is changed), with no
# window, no audio, two virtual CPUs and GLES rendered inside the guest (-gpu guest), and its Wi-Fi and mobile data are switched off before the museum starts. The
# museum's own log lines come from logcat (tag "museum"). It passes only when the run reached its final stats line with
# 0 exhibit and 0 script errors, every room was visited, no renderer self-check failed and the process did not crash.
set -euo pipefail

cd "$(dirname "$0")/.."
sdk=${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}}
adb=$sdk/platform-tools/adb
emulator=$sdk/emulator/emulator
avd=${MUSEUM_ANDROID_AVD:-Medium_Phone}
port=${MUSEUM_ANDROID_PORT:-5584}
serial=emulator-$port
apk=build-consumer/android-gradle/outputs/apk/debug/app-debug.apk
native=${MUSEUM_BIN:-build/bin/cna-3d-museum}
package=org.libcna.museum
out=build-consumer/android-run
remote=/sdcard/Android/data/$package/files/run.script

[ -s "$apk" ] || { echo "error: $apk not found; run tools/build_android.sh first" >&2; exit 2; }
[ -x "$emulator" ] && [ -x "$adb" ] || { echo "error: emulator or adb not found under $sdk" >&2; exit 2; }
[ -r /dev/kvm ] && [ -w /dev/kvm ] || { echo "error: /dev/kvm is not usable; the x86_64 emulator needs it" >&2; exit 2; }
mkdir -p "$out"

rooms=() skipped=()
if [ -n "${MUSEUM_ANDROID_SCRIPT:-}" ]; then
  script=$MUSEUM_ANDROID_SCRIPT
else
  [ -x "$native" ] || { echo "error: $native (for the room list) is not built" >&2; exit 2; }
  if [ $# -gt 0 ]; then rooms=("$@"); else mapfile -t rooms < <("$native" --list-rooms); fi
  # MUSEUM_ANDROID_SKIP="room ..." leaves rooms out, named in the result (never silently): for an emulator defect that ends the run.
  skipped=()
  if [ -n "${MUSEUM_ANDROID_SKIP:-}" ]; then
    kept=()
    for k in "${rooms[@]}"; do
      if [[ " $MUSEUM_ANDROID_SKIP " == *" $k "* ]]; then skipped+=("$k"); else kept+=("$k"); fi
    done
    rooms=("${kept[@]}")
  fi
fi
# The tour runs in batches (MUSEUM_ANDROID_BATCH rooms, default 80), one start of the app each, on the same emulator: in one
# process the emulator's own qemu crashed (SIGSEGV, core dumped) at the same point every time, about 167 rooms in.
scripts=()
if [ ${#rooms[@]} -eq 0 ]; then
  scripts=("$script")
else
  batch=${MUSEUM_ANDROID_BATCH:-80}
  for ((i = 0; i < ${#rooms[@]}; i += batch)); do
    script=""
    for k in "${rooms[@]:i:batch}"; do
      script+="goto:$k;wait:10;"
      [ "${MUSEUM_ANDROID_PRESSALL:-0}" = 1 ] && script+="pressall;wait:4;"
    done
    scripts+=("${script}stats;quit")
  done
fi

# A read-only emulator keeps its overlay disks in /tmp/android-$USER and deletes them on a normal exit only; the marker lets the
# cleanup remove exactly this run's, when no process holds them any more.
marker=$(mktemp "$out/.started.XXXXXX")
# -memory: the shared AVD's 2 GB left the guest short of memory; -cores: two virtual CPUs instead of the AVD's four, as every
# job on this shared machine is held to two cores. Both apply to this run only and do not change the AVD.
"$emulator" -avd "$avd" -read-only -no-window -no-audio -no-boot-anim -no-snapshot -gpu "${MUSEUM_ANDROID_GPU:-guest}" \
  -memory "${MUSEUM_ANDROID_MEMORY:-4096}" -cores "${MUSEUM_ANDROID_CORES:-2}" -port "$port" > "$out/emulator.log" 2>&1 &
emulator_pid=$!
adb_() { timeout "${ADB_TIMEOUT:-60}" "$adb" -s "$serial" "$@"; }   # an adb call never waits for a device that is gone
cleanup() {
  adb_ emu kill >/dev/null 2>&1 || true
  sleep 3
  pkill -f "qemu-system.*-port $port" 2>/dev/null || true   # killing the launcher alone leaves its qemu running
  kill "$emulator_pid" 2>/dev/null || true
  sleep 3
  find "/tmp/android-$USER" -maxdepth 1 -name 'emulator-*' -newer "$marker" 2>/dev/null | while read -r f; do
    fuser -s "$f" 2>/dev/null || rm -f "$f"
  done
  rm -f "$marker"
}
trap cleanup EXIT
ADB_TIMEOUT=300 adb_ wait-for-device || { echo "error: the emulator did not come up (log: $out/emulator.log)" >&2; exit 1; }
for _ in $(seq 1 150); do
  [ "$(adb_ shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = 1 ] && break
  sleep 2
done
[ "$(adb_ shell getprop sys.boot_completed | tr -d '\r')" = 1 ] || { echo "error: the emulator did not boot (log: $out/emulator.log)" >&2; exit 1; }
adb_ shell svc wifi disable || true
adb_ shell svc data disable || true
ADB_TIMEOUT=300 adb_ install -r "$apk" > "$out/install.log" 2>&1 || { cat "$out/install.log"; exit 1; }
# The app's external files directory exists once the activity created it; a first start without a script makes it.
adb_ shell mkdir -p "$(dirname "$remote")" 2>/dev/null || true
adb_ logcat -c
: > "$out/logcat.txt"
# The whole log is streamed to the host as it is written, so a guest that hangs or restarts mid-run still leaves its last lines
# (the device's ring buffer would also drop the earliest museum lines of a long tour).
( while kill -0 "$emulator_pid" 2> /dev/null; do "$adb" -s "$serial" logcat -v threadtime 2> /dev/null; sleep 2; done ) >> "$out/logcat.txt" &
streamer=$!
start=$(date +%s)
deadline=$((start + ${MUSEUM_ANDROID_TIMEOUT:-3600}))
emulator_gone=0 device_lost=0 timed_out=0
: > "$out/start.log"
for ((b = 0; b < ${#scripts[@]}; ++b)); do
  printf '%s' "${scripts[b]}" > "$out/run.script"
  adb_ push "$out/run.script" "$remote" > /dev/null
  adb_ shell am start -W -n "$package/.MuseumActivity" --es scriptfile "$remote" >> "$out/start.log"
  sleep 5
  # SDL's activity process outlives the museum's main(): a batch is over once the museum says it is exiting (or it died).
  while :; do
    [ "$(date +%s)" -lt $deadline ] || { timed_out=1; break; }
    # The launcher outlives a crashed qemu, so the emulator counts as running only while its qemu (found by port) does.
    pgrep -f "qemu-system.*-port $port" > /dev/null 2>&1 || { emulator_gone=1; break; }
    if ! adb_ shell pidof "$package" > /dev/null 2>&1; then
      # Not running, or the device is briefly offline: only a device that stays away for two minutes ends the run as lost.
      ADB_TIMEOUT=120 adb_ wait-for-device 2> /dev/null || { device_lost=1; break; }
      adb_ shell pidof "$package" > /dev/null 2>&1 || break
    fi
    [ "$(grep -ac 'Game.Exiting raised' "$out/logcat.txt")" -gt "$b" ] && { sleep 3; break; }
    sleep 5
  done
  adb_ shell am force-stop "$package" 2> /dev/null || true
  [ $emulator_gone = 0 ] && [ $device_lost = 0 ] && [ $timed_out = 0 ] || break
  sleep 2
done
sleep 2
pkill -P "$streamer" 2> /dev/null || true; kill "$streamer" 2> /dev/null || true; wait "$streamer" 2> /dev/null || true
grep -a ' museum *: ' "$out/logcat.txt" | sed -E 's/^.* museum *: //' > "$out/museum.log" || true
seconds=$(( $(date +%s) - start ))

# One stats line per batch: each must be clean, and the visited counts add up over the batches.
mapfile -t all_stats < <(grep -a '\[museum\]\[stats\]' "$out/museum.log" || true)
stats=""
[ ${#all_stats[@]} -eq 0 ] || stats=${all_stats[${#all_stats[@]} - 1]}
# Rooms entered, as tools/tour.sh counts them natively: one goto line each (an unknown room is a script error instead).
visited=$(grep -ac '^\[museum\]\[script\] goto ' "$out/museum.log" || true)
problems=()
[ $timed_out = 0 ] || problems+=("the run passed MUSEUM_ANDROID_TIMEOUT (${MUSEUM_ANDROID_TIMEOUT:-3600} s)")
[ ${#all_stats[@]} -ge ${#scripts[@]} ] || problems+=("${#all_stats[@]} of ${#scripts[@]} batch(es) reached their final stats line")
for line in "${all_stats[@]}"; do
  echo "$line" | grep -q 'exhibit errors 0 .*script errors 0' || problems+=("errors in a stats line: ${line: -60}")
done
[ $device_lost = 0 ] || problems+=("adb lost the device for two minutes after $seconds s (the guest hung or restarted; its last lines: $out/logcat.txt)")
[ $emulator_gone = 0 ] || problems+=("the emulator exited during the run, after $seconds s (log: $out/emulator.log)")
[ ${#rooms[@]} -eq 0 ] || [ "${visited:-0}" -ge "${#rooms[@]}" ] || problems+=("visited ${visited:-0} of ${#rooms[@]} rooms")
if grep -aqE '\[museum\]\[(exhibit|draw|script)-error\]' "$out/museum.log"; then
  problems+=("$(grep -aE '\-error\]' "$out/museum.log" | sed -n 1p | cut -c1-160)")
fi
if grep -aqE "Fatal signal|FATAL EXCEPTION|>>> $package <<<" "$out/logcat.txt"; then
  problems+=("the app crashed: $(grep -aE 'Fatal signal|FATAL EXCEPTION|Abort message' "$out/logcat.txt" | sed -n 1p | cut -c1-160)")
fi
validate_fail=$(grep -acE '\[museum\]\[validate\] .*: FAIL' "$out/museum.log" || true)
validate_pass=$(grep -acE '\[museum\]\[validate\] .*: PASS' "$out/museum.log" || true)
[ "${validate_fail:-0}" = 0 ] || problems+=("$validate_fail renderer self-check(s) failed: $(grep -aE '\[validate\] .*: FAIL' "$out/museum.log" | sed -n 1p | cut -c1-140)")
if [ -n "${MUSEUM_ANDROID_EXPECT:-}" ]; then
  IFS='|' read -ra expected <<< "$MUSEUM_ANDROID_EXPECT"
  for e in "${expected[@]}"; do grep -aqF -- "$e" "$out/museum.log" || problems+=("missing: $e"); done
fi

renderer_line=$(grep -a 'on adapter' "$out/museum.log" | sed -n 1p || true)
[ ${#skipped[@]} -eq 0 ] || echo "left out on purpose: ${skipped[*]}"
echo "android ($avd, ${renderer_line:-renderer line missing}): ${#rooms[@]} rooms, visited ${visited:-0}, ${seconds} s; renderer self-checks $validate_pass PASS, ${validate_fail:-0} FAIL (log: $out/museum.log)"
if [ ${#problems[@]} -eq 0 ]; then
  echo "ANDROID RUN: PASS"
  [ ${#rooms[@]} -eq 0 ] || BUILD_DIR=build-consumer python3 tools/record_profile.py android-tour --passed 1 --failed 0 --binary "$apk" --library "$apk" --not-run ${#skipped[@]} --note "$validate_pass self-checks PASS, emulator $avd${skipped:+; left out: ${skipped[*]}}" || true
else
  printf 'FAIL  %s\n' "${problems[@]}"
  echo "ANDROID RUN: FAIL"
  [ ${#rooms[@]} -eq 0 ] || BUILD_DIR=build-consumer python3 tools/record_profile.py android-tour --passed 0 --failed 1 --binary "$apk" --library "$apk" --note "${problems[0]}" || true
  exit 1
fi
