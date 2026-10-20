#!/usr/bin/env bash
# Resource-lifetime check: tours every room once and three times and compares the peak memory and the number of
# registered textures. Rooms are built and destroyed as the visitor moves, so a second and third tour must not grow
# the process much (a leak would grow by about the same amount per tour). Exit status 1 when it grows more than LIMIT %.
#   tools/leak_check.sh [RENDERER ...]       no argument: the default/CNA_GRAPHICS_RENDERER selection
set -u
cd "$(dirname "$0")/.." || exit 2
bin=${MUSEUM_BIN:-${BUILD_DIR:-build}/bin/cna-3d-museum}
limit=${LIMIT:-25}
command -v /usr/bin/time >/dev/null || { echo "needs /usr/bin/time"; exit 2; }
mapfile -t keys < <("$bin" --list-rooms)
run() {   # run RENDERER N TAG -> prints "peak_kb textures exhibit_errors"
  local renderer=$1 tours=$2 tag=$3 script="" r k log script_file peak textures errors timeout_seconds
  local -a launch=(/usr/bin/time -v env)
  for ((r = 0; r < tours; ++r)); do for k in "${keys[@]}"; do script+="goto:$k;wait:2;"; done; done
  script+="stats;quit"
  log="${BUILD_DIR:-build}/leak_${tag}_${tours}.log"
  script_file="${BUILD_DIR:-build}/leak_${tag}_${tours}.script"
  printf '%s' "$script" > "$script_file"
  # run.sh's five-minute default is ample for one tour but can expire near the
  # end of the three-tour sample on a busy software-rendering host.  Scale the
  # default with the requested work while preserving an explicit caller limit.
  timeout_seconds=${MUSEUM_TIMEOUT:-$((300 * tours))}
  launch+=(MUSEUM_TIMEOUT="$timeout_seconds")
  [ -n "$renderer" ] && launch+=(CNA_GRAPHICS_RENDERER="$renderer")
  launch+=(tools/run.sh "@$script_file" --no-audio)
  if ! "${launch[@]}" > "$log" 2>&1; then
    echo "leak check $tag run $tours failed before comparison (log: $log)" >&2
    return 1
  fi
  read -r peak textures errors < <(awk '
    /\[museum\]\[stats\]/ {
      for (i = 1; i <= NF; ++i) {
        if ($i == "textures") textures = $(i + 1)
        if ($i == "exhibit" && $(i + 1) == "errors") errors = $(i + 2)
        if ($i == "script" && $(i + 1) == "errors") scripted = $(i + 2)
      }
    }
    /Maximum resident set size/ { peak = $NF }
    # A script error (a room the tour could not reach) counts as an error too: the tour did not measure what it says.
    END { if (peak != "" && textures != "" && errors != "") print peak, textures, errors + scripted }
  ' "$log")
  if [ -z "${peak:-}" ] || [ -z "${textures:-}" ] || [ -z "${errors:-}" ]; then
    echo "leak check $tag run $tours has no final stats or peak memory (log: $log)" >&2
    return 1
  fi
  printf '%s %s %s\n' "$peak" "$textures" "$errors"
}

check_renderer() {
  local renderer=$1 tag=${1:-default} result1 result3 peak1 peak3 tex1 tex3 errors1 errors3 growth
  [[ "$tag" =~ ^[A-Za-z0-9_-]+$ ]] || { echo "invalid renderer name: $tag" >&2; return 2; }
  echo "$tag: measuring one complete tour..." >&2
  result1=$(run "$renderer" 1 "$tag") || return $?
  echo "$tag: measuring three tours in one process..." >&2
  result3=$(run "$renderer" 3 "$tag") || return $?
  read -r peak1 tex1 errors1 <<< "$result1"
  read -r peak3 tex3 errors3 <<< "$result3"
  growth=$(( (peak3 - peak1) * 100 / peak1 ))
  echo "$tag: one tour peak $((peak1 / 1024)) MB, $tex1 textures;  three tours peak $((peak3 / 1024)) MB, $tex3 textures;  growth $growth %, exhibit errors $errors1/$errors3"
  if [ "$growth" -gt "$limit" ] || [ "$tex1" != "$tex3" ] || [ "$errors1" != 0 ] || [ "$errors3" != 0 ]; then
    echo "FAIL: $tag memory/textures grew or an exhibit threw across tours"
    return 1
  fi
  echo "$tag: ok"
}

if [ $# -eq 0 ]; then
  check_renderer ""
else
  for renderer in "$@"; do check_renderer "$renderer" || exit $?; done
fi
