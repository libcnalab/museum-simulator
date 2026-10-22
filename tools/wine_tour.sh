#!/usr/bin/env bash
# The Windows build under Wine on a private virtual display: every room (or the ones named) and, with -p, every control,
# as tools/tour.sh does natively. Needs the MinGW tree from docs/RENDERER_TESTING.md ("Windows: the MinGW build under Wine").
#
#   tools/wine_tour.sh [-p] [room ...]     renderer from CNA_GRAPHICS_RENDERER (default SOFTWARE; DIRECTX9 runs on Wine's D3D)
#
# It passes only when the museum exited 0, reached its final stats line with 0 exhibit and 0 script errors, and entered every
# requested room. The result is Wine's, recorded as the profile wine-<renderer>-tour, never as a Windows result.
set -u
cd "$(dirname "$0")/.." || exit 2
press=0
[ "${1:-}" = -p ] && { press=1; shift; }
dir=${BUILD_DIR:-build-consumer/windows}
exe=$dir/bin/cna-3d-museum.exe
native=${MUSEUM_BIN:-build/bin/cna-3d-museum}
renderer=${CNA_GRAPHICS_RENDERER:-SOFTWARE}
[ -f "$exe" ] || { echo "error: $exe is not built (docs/RENDERER_TESTING.md has the MinGW recipe)" >&2; exit 2; }
[ -x "$native" ] || { echo "error: $native (for the room list and the compiled content) is not built" >&2; exit 2; }
command -v wine > /dev/null && command -v Xvfb > /dev/null || { echo "error: wine and Xvfb are needed" >&2; exit 2; }
xvfb=$PWD/tools/private_xvfb.sh

# The content tool is a foreign executable in this tree, so the compiled content comes from the native build; the MinGW
# runtime libraries sit next to the executable.
[ -d "$dir/bin/Content/Compiled" ] || cp -r "$(dirname "$native")/Content/Compiled" "$dir/bin/Content/"
for dll in /usr/lib/gcc/x86_64-w64-mingw32/*-win32/libgcc_s_seh-1.dll /usr/lib/gcc/x86_64-w64-mingw32/*-win32/libstdc++-6.dll /usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll; do
  [ -f "$dir/bin/$(basename "$dll")" ] || cp "$dll" "$dir/bin/"
done

if [ $# -gt 0 ]; then rooms=("$@"); else mapfile -t rooms < <("$native" --list-rooms); fi
script=""
for k in "${rooms[@]}"; do script+="goto:$k;wait:3;"; [ $press = 1 ] && script+="pressall;wait:4;"; done
printf '%s' "${script}stats;quit" > "$dir/wine-tour.script"
log=$dir/wine-tour-$renderer.log
start=$(date +%s)
# The script file is given relative to the working directory, which Wine maps as it is.
(cd "$dir/bin" && WINEPREFIX=${WINEPREFIX:-$HOME/.cache/museum-wine} WINEDEBUG=-all SDL_VIDEODRIVER=windows CNA_GRAPHICS_RENDERER=$renderer \
  timeout -k 15 "${MUSEUM_TIMEOUT:-5400}" "$xvfb" -s "-screen 0 1280x720x24" wine cna-3d-museum.exe --script-file ../wine-tour.script --no-audio) > "$log" 2>&1
status=$?
seconds=$(( $(date +%s) - start ))
stats=$(grep -a '\[museum\]\[stats\]' "$log" | tail -1)
visited=$(grep -ac '^\[museum\]\[script\] goto ' "$log")
problems=()
[ $status = 0 ] || problems+=("the museum exited with status $status")
echo "$stats" | grep -q 'exhibit errors 0 .*script errors 0' || problems+=("stats line: ${stats: -70}")
[ "$visited" -ge ${#rooms[@]} ] || problems+=("entered $visited of ${#rooms[@]} rooms")
grep -aqE '\[museum\]\[(exhibit|draw|script)-error\]' "$log" && problems+=("$(grep -aE '\-error\]' "$log" | head -1 | cut -c1-160)")
echo "wine $renderer tour$([ $press = 1 ] && echo ' with every control'): ${#rooms[@]} rooms, entered $visited, $seconds s (log: $log)"
profile=wine-$(echo "$renderer" | tr '[:upper:]' '[:lower:]')-tour
if [ ${#problems[@]} -eq 0 ]; then
  echo "WINE TOUR: PASS"
  BUILD_DIR=$dir python3 tools/record_profile.py "$profile" --passed 1 --failed 0 --binary "$exe" --library "$exe" --note "Wine, not Windows; $seconds s" || true
else
  printf 'FAIL  %s\n' "${problems[@]}"
  echo "WINE TOUR: FAIL"
  BUILD_DIR=$dir python3 tools/record_profile.py "$profile" --passed 0 --failed 1 --binary "$exe" --library "$exe" --note "${problems[0]}" || true
  exit 1
fi
