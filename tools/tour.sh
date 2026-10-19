#!/usr/bin/env bash
# Visits every room (or the ones named) for a few frames each on a private virtual display and reports
# errors, exceptions and the museum's own resource counters. Exit status 0 only if nothing went wrong.
#   tools/tour.sh [-f FRAMES] [-p] [-z N] [-s SEED] [room-key ...]      -p: also operate every control of every room (smoke test)
#                                                       -z N: then N random control actions per room (fuzz; repeatable; -s SEED picks the sequence; the script goes through a file, so N is not limited by the command line)
set -u
cd "$(dirname "$0")/.." || exit 2
frames=12
press=0
fuzz=0
seed=0
while [ $# -gt 0 ]; do
  case "$1" in
    -f) frames=$2; shift 2 ;;
    -p) press=1; shift ;;
    -z) fuzz=$2; shift 2 ;;
    -s) seed=$2; shift 2 ;;
    *) break ;;
  esac
done
bin=${MUSEUM_BIN:-${BUILD_DIR:-build}/bin/cna-3d-museum}
if [ $# -gt 0 ]; then keys=("$@"); else mapfile -t keys < <("$bin" --list-rooms); fi
script=""
for k in "${keys[@]}"; do script+="goto:$k;wait:$frames;"; [ $press = 1 ] && script+="pressall;wait:4;"; for ((z = 0; z < fuzz; ++z)); do script+="fuzz:${seed};wait:2;"; done; done
script+="stats;quit"
echo "touring ${#keys[@]} rooms, $frames frames each ..."
start=$(date +%s)
# The script goes through a file: with many rooms and fuzz steps it outgrows the longest command-line argument the kernel allows.
printf '%s' "$script" > "${BUILD_DIR:-build}/tour.script"
MUSEUM_TIMEOUT=${MUSEUM_TIMEOUT:-1500} tools/run.sh "@${BUILD_DIR:-build}/tour.script" --no-audio > "${BUILD_DIR:-build}/tour.log" 2>&1
status=$?
end=$(date +%s)
grep -E "\[museum\]\[(script|stats|main)\]|exception|ERROR|FAILED|INVALID|Assertion" "${BUILD_DIR:-build}/tour.log" | grep -v "^\[museum\]\[models\]" | grep -v "pressall: [0-9]* panel" | tail -15
completed=$(grep -c "\[museum\]\[stats\]" "${BUILD_DIR:-build}/tour.log")
if [ "$completed" -eq 0 ]; then
  echo "tour ended before its final stats step"
  [ $status -eq 0 ] && status=4
fi
# Every requested room must have been entered, in order: a room that disappeared from the catalogue is a failure, not a skip.
visited=$(grep -c "^\[museum\]\[script\] goto " "${BUILD_DIR:-build}/tour.log")
if [ "$visited" -ne "${#keys[@]}" ]; then
  echo "only $visited of ${#keys[@]} requested rooms were entered"
  [ $status -eq 0 ] && status=3
fi
scripterr=$(grep -c "\[museum\]\[script-error\]" "${BUILD_DIR:-build}/tour.log")
if [ "$scripterr" -gt 0 ]; then
  echo "$scripterr script error(s) (unknown room or command, a control that does not exist):"
  grep "\[museum\]\[script-error\]" "${BUILD_DIR:-build}/tour.log" | cut -c1-200 | sort -u | head -10
  [ $status -eq 0 ] && status=3
fi
caught=$(grep -c "\[museum\]\[exhibit-error\]" "${BUILD_DIR:-build}/tour.log")
if [ "$caught" -gt 0 ]; then
  echo "$caught exhibit exception(s) were caught (the museum carried on, but a room is broken on this renderer):"
  grep "\[museum\]\[exhibit-error\]" "${BUILD_DIR:-build}/tour.log" | cut -c1-240 | sort -u | head -10
  [ $status -eq 0 ] && status=3
fi
drawn=$(grep -c "\[museum\]\[draw-error\]" "${BUILD_DIR:-build}/tour.log")
if [ "$drawn" -gt 0 ]; then
  echo "$drawn draw error(s) (distinct messages; the museum kept running but something was not drawn):"
  grep "\[museum\]\[draw-error\]" "${BUILD_DIR:-build}/tour.log" | cut -c1-260 | sort -u | head -10
  [ $status -eq 0 ] && status=3
fi
echo "exit status $status, $((end - start)) s"
exit $status
