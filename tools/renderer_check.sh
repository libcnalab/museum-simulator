#!/usr/bin/env bash
# Runs the Renderer Validation rooms (floor 9) on each given renderer and prints a PASS/FAIL matrix.
#   tools/renderer_check.sh [RENDERER ...]        default: every renderer the build contains that works here
# The checks are the museum's own rv-* rooms: every line "[museum][validate] <room> <check>: PASS|FAIL|INFO|SKIP"
# in the log is one cell of the matrix. Exit status 0 requires a result from every room, the final stats marker,
# no renderer/check/exhibit/draw failure, and a successful museum process.
# MUSEUM_GPU=1 runs on the machine's real GPU with no display (see tools/_headless.sh); results are named <RENDERER>-gpu.
set -u
cd "$(dirname "$0")/.." || exit 2
renderers=("$@")
[ ${#renderers[@]} -gt 0 ] || renderers=(OPENGLES3 OPENGL33 SOFTWARE)
bin=${MUSEUM_BIN:-${BUILD_DIR:-build}/bin/cna-3d-museum}
[ -x "$bin" ] || { echo "error: $bin not found; build first" >&2; exit 2; }
room_output=$("$bin" --list-rooms)
room_rc=$?
[ $room_rc -eq 0 ] || { echo "error: $bin --list-rooms exited $room_rc" >&2; exit 2; }
rooms=()
while IFS= read -r room; do
  [[ $room == rv-* ]] && rooms+=("$room")
done <<< "$room_output"
[ ${#rooms[@]} -gt 0 ] || { echo "error: the museum listed no rv-* validation rooms" >&2; exit 2; }
script=""
for r in "${rooms[@]}"; do script+="goto:$r;wait:10;"; done
script+="stats;quit"
out=${BUILD_DIR:-build}/renderer_check
mkdir -p "$out"
printf '%s\n' "${rooms[@]}" > "$out/expected-rooms.txt"
status=0
for r in "${renderers[@]}"; do
  name=$r
  [ -n "${MUSEUM_GPU:-}" ] && name=$r-gpu
  CNA_GRAPHICS_RENDERER=$r MUSEUM_TIMEOUT=${MUSEUM_TIMEOUT:-600} tools/run.sh "$script" > "$out/$name.console" 2>&1
  rc=$?
  run_log=${BUILD_DIR:-build}/run.log
  if [ ! -f "$run_log" ]; then
    echo "$name: museum exit $rc; FAIL: tools/run.sh produced no $run_log" >&2
    : > "$out/$name.log"
    : > "$out/$name.checks"
    status=1
    continue
  fi
  cp "$run_log" "$out/$name.log"
  if [ -n "${MUSEUM_GPU:-}" ]; then
    # The museum cannot tell which driver draws; Mesa's EGL and the Vulkan loader can: keep what they chose.
    EGL_LOG_LEVEL=debug VK_LOADER_DEBUG=driver CNA_GRAPHICS_RENDERER=$r MUSEUM_LOG=$out/$name.device-run.log \
      tools/run.sh "wait:5;quit" > /dev/null 2>&1
    grep -E "libEGL debug: (using driver|pci id)|Loading driver|Found ICD|ICD .*(chosen|added)|on adapter" "$out/$name.device-run.log" \
      | sort -u > "$out/$name.device"
    rm -f "$out/$name.device-run.log" "$out/$name.device-run.log.lock"
  fi
  summary=$(python3 tools/validate_renderer_log.py --rooms "$out/expected-rooms.txt" \
      --log "$out/$name.log" --checks "$out/$name.checks" 2> "$out/$name.validation-errors")
  validation_rc=$?
  printf "%-10s exit %-3s  %s\n" "$name" "$rc" "$summary"
  if [ -s "$out/$name.validation-errors" ]; then
    sed "s/^/             /" "$out/$name.validation-errors" >&2
  fi
  [ "$validation_rc" -ne 0 ] && status=1
  [ "$rc" -ne 0 ] && status=1
done
echo "per-renderer results: $out/<RENDERER>[-gpu].checks"
exit $status
