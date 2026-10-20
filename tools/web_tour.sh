#!/usr/bin/env bash
# The whole museum in a browser (WEBGL2): every room visited and every control operated, as tools/tour.sh -f 3 -p does
# natively, through web/shell.html's ?script= parameter, in headless Chrome with SwiftShader.
#
#   tools/web_tour.sh            exit 0 when the tour finished cleanly; needs tools/build_web.sh first
#
# It passes only when the page reported exit 0, the final stats line has 0 exhibit and 0 script errors and every room
# was visited, no renderer self-check failed, and nothing aborted, threw on the page or lost the WebGL context.
# MUSEUM_WEB_TIMEOUT (seconds, default 3600) bounds the browser; the profile is recorded as web-webgl2-tour.
set -euo pipefail

cd "$(dirname "$0")/.."
build_dir=${BUILD_DIR:-build-consumer}
page="$build_dir/bin/cna-3d-museum.html"
chrome=${MUSEUM_CHROME:-google-chrome}
port=${MUSEUM_WEB_PORT:-8735}
native=${MUSEUM_BIN:-build/bin/cna-3d-museum}

[ -s "$page" ] || { echo "error: $page not found; run tools/build_web.sh first" >&2; exit 2; }
[ -x "$native" ] || { echo "error: $native (for the room list) is not built" >&2; exit 2; }
command -v "$chrome" >/dev/null 2>&1 || { echo "error: browser '$chrome' not found" >&2; exit 2; }

mapfile -t rooms < <("$native" --list-rooms)
script=""
for k in "${rooms[@]}"; do script+="goto:$k;wait:3;pressall;wait:4;"; done
script+="stats;quit"
query=$(python3 -I -c 'import sys, urllib.parse; print(urllib.parse.quote(sys.argv[1], safe=""))' "$script")

out="$build_dir/web-tour"
mkdir -p "$out"
log="$out/chrome.log"
profile="$out/chrome-profile"
rm -rf "$profile"
python3 -m http.server "$port" --directory "$build_dir/bin" --bind 127.0.0.1 >/dev/null 2>&1 &
server_pid=$!
cleanup() { kill "$server_pid" 2>/dev/null || true; rm -rf "$profile"; }
trap cleanup EXIT
for _ in $(seq 1 50); do
  curl -fsS -o /dev/null "http://127.0.0.1:$port/cna-3d-museum.html" 2>/dev/null && break
  sleep 0.1
done
kill -0 "$server_pid" 2>/dev/null || { echo "error: the HTTP server did not start on port $port (MUSEUM_WEB_PORT)" >&2; exit 2; }

timeout -k 15 "${MUSEUM_WEB_TIMEOUT:-3600}" "$chrome" --headless=new --no-sandbox --disable-gpu-sandbox \
  --enable-webgl --enable-unsafe-swiftshader --use-angle=swiftshader \
  --user-data-dir="$profile" --enable-logging=stderr --v=1 \
  "http://127.0.0.1:$port/cna-3d-museum.html?script=$query" >"$log" 2>&1 || true

# The museum's own lines, as the console carried them.
grep -aE 'CONSOLE.*MUSEUM_(OUT|ERR|ABORT|EXIT|PAGE_ERROR)' "$log" | sed -E 's/^.*CONSOLE[^"]*"//; s/", source.*$//' > "$out/museum.log" || true
stats=$(grep -a '\[museum\]\[stats\]' "$out/museum.log" | tail -1 || true)
visited=$(echo "$stats" | grep -oE 'visited [0-9]+' | grep -oE '[0-9]+' || echo 0)
problems=()
grep -aq 'MUSEUM_EXIT 0' "$out/museum.log" || problems+=("the page did not report exit 0 ($(grep -a 'MUSEUM_EXIT\|MUSEUM_ABORT' "$out/museum.log" | tail -1))")
echo "$stats" | grep -q 'exhibit errors 0 .*script errors 0' || problems+=("errors in the stats line: ${stats: -60}")
[ "${visited:-0}" -ge "${#rooms[@]}" ] || problems+=("visited ${visited:-0} of ${#rooms[@]} rooms")
if grep -aqE 'MUSEUM_(ABORT|PAGE_ERROR)|WebGL context.*lost|\[museum\]\[(exhibit|draw|script)-error\]' "$out/museum.log"; then
  problems+=("$(grep -aE 'MUSEUM_(ABORT|PAGE_ERROR)|context.*lost|-error\]' "$out/museum.log" | head -1 | cut -c1-160)")
fi
validate_fail=$(grep -acE '\[museum\]\[validate\] .*: FAIL' "$out/museum.log" || true)
validate_pass=$(grep -acE '\[museum\]\[validate\] .*: PASS' "$out/museum.log" || true)
[ "${validate_fail:-0}" = 0 ] || problems+=("$validate_fail renderer self-check(s) failed: $(grep -aE '\[validate\] .*: FAIL' "$out/museum.log" | head -1 | cut -c1-140)")

echo "WEBGL2 tour: ${#rooms[@]} rooms, visited ${visited:-0}; renderer self-checks $validate_pass PASS, ${validate_fail:-0} FAIL (log: $out/museum.log)"
if [ ${#problems[@]} -eq 0 ]; then
  echo "WEBGL2 BROWSER TOUR: PASS"
  BUILD_DIR=$build_dir python3 tools/record_profile.py web-webgl2-tour --passed 1 --failed 0 --binary "$build_dir/bin/cna-3d-museum.wasm" --library "$build_dir/bin/cna-3d-museum.wasm" --note "$validate_pass self-checks PASS" || true
else
  printf 'FAIL  %s\n' "${problems[@]}"
  echo "WEBGL2 BROWSER TOUR: FAIL"
  BUILD_DIR=$build_dir python3 tools/record_profile.py web-webgl2-tour --passed 0 --failed 1 --binary "$build_dir/bin/cna-3d-museum.wasm" --library "$build_dir/bin/cna-3d-museum.wasm" --note "${problems[0]}" || true
  exit 1
fi
