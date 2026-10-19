# Shared helper, sourced by run.sh and shot.sh.
#
# museum_headless OUT_LOG ARGS...
#   Runs the museum on a private virtual X display (never the live desktop) with software GL and a dummy
#   audio driver unless the caller already chose otherwise, writes everything to OUT_LOG and returns the
#   museum's own exit status (124 = timed out). Nothing is swallowed: a crash is a nonzero return.
#
# The timeout escalates to SIGKILL 15 s after SIGTERM: SDL turns SIGTERM into a quit event, which a museum blocked in a
# modal dialog or a hung driver call never reads.
# Environment: MUSEUM_BIN (binary), MUSEUM_TIMEOUT (seconds, default 300), MUSEUM_AUDIO=real (do not force
#   the dummy audio driver), CNA_GRAPHICS_RENDERER (pick a renderer in a multi-renderer build),
#   MUSEUM_SCREEN (Xvfb screen, default 1920x1080x24), MUSEUM_RENDER_THREADS (default 3: threads Mesa's software renderer may use),
#   MUSEUM_GPU=1 (no display at all: SDL's offscreen video driver on the machine's real GPU, EGL through the render node
#   and Vulkan through VK_EXT_headless_surface; pick the Vulkan driver with VK_ICD_FILENAMES).
museum_root() { cd "$(dirname "${BASH_SOURCE[1]}")/.." && pwd; }

museum_headless() {
  local log=$1; shift
  local root; root=$(museum_root)
  local bin=${MUSEUM_BIN:-$root/${BUILD_DIR:-build}/bin/cna-3d-museum}
  if [ ! -x "$bin" ]; then
    echo "error: $bin not found; run tools/build.sh first" >&2
    return 2
  fi
  local gpu=${MUSEUM_GPU:-}
  [ -n "$gpu" ] || command -v xvfb-run >/dev/null 2>&1 || { echo "error: xvfb-run is required (package xvfb)" >&2; return 2; }
  mkdir -p "$(dirname "$log")"
  # Two runs writing the same log would interleave and each would read the other's result: the second one is refused.
  # (Point BUILD_DIR or MUSEUM_LOG elsewhere to run side by side.) The lock is released when this run ends, however it ends.
  local museum_lock=
  if command -v flock >/dev/null 2>&1; then
    exec {museum_lock}> "$log.lock"
    if ! flock -n "$museum_lock"; then
      echo "error: another museum run is writing $log; wait for it or use a different BUILD_DIR/MUSEUM_LOG" >&2
      exec {museum_lock}>&-
      return 2
    fi
  fi
  local audio=dummy
  [ "${MUSEUM_AUDIO:-dummy}" = real ] && audio=
  # Test runs keep CNA's local gamer profiles and achievements in the build tree, never in the user's own store.
  # Software rendering (Mesa llvmpipe/lavapipe) otherwise spreads one museum over every core: keep it to three (MUSEUM_RENDER_THREADS).
  local status
  if [ -n "$gpu" ]; then
    # Offscreen: there is no window anywhere, so nothing reaches the live desktop, and the GPU renders for real.
    env -u WAYLAND_DISPLAY -u DISPLAY -u LIBGL_ALWAYS_SOFTWARE SDL_VIDEODRIVER=offscreen ${audio:+SDL_AUDIODRIVER=$audio} \
      CNA_GAMER_SERVICES_PROFILES_DIR="${CNA_GAMER_SERVICES_PROFILES_DIR:-$root/${BUILD_DIR:-build}/gamer-profiles}" \
      timeout -k 15 "${MUSEUM_TIMEOUT:-300}" "$bin" "$@" > "$log" 2>&1
    status=$?
  else
    env -u WAYLAND_DISPLAY SDL_VIDEODRIVER=x11 ${audio:+SDL_AUDIODRIVER=$audio} LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS="${LP_NUM_THREADS:-${MUSEUM_RENDER_THREADS:-3}}" \
      CNA_GAMER_SERVICES_PROFILES_DIR="${CNA_GAMER_SERVICES_PROFILES_DIR:-$root/${BUILD_DIR:-build}/gamer-profiles}" \
      timeout -k 15 "${MUSEUM_TIMEOUT:-300}" \
      xvfb-run -a -e "$log.xvfb" -s "-screen 0 ${MUSEUM_SCREEN:-1920x1080x24}" "$bin" "$@" > "$log" 2>&1
    status=$?
  fi
  [ -n "${museum_lock:-}" ] && exec {museum_lock}>&-
  grep -v '^\[INFO\]' "$log"
  return $status
}
