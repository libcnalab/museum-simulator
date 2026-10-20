#!/usr/bin/env bash
# Serve the WebGL 2 bundle and verify a short real-Chrome run.
set -euo pipefail

cd "$(dirname "$0")/.."
build_dir=${BUILD_DIR:-build-consumer}
page="$build_dir/bin/cna-3d-museum.html"
chrome=${MUSEUM_CHROME:-google-chrome}
port=${MUSEUM_WEB_PORT:-8734}

[ -s "$page" ] || { echo "error: $page not found; run tools/build_web.sh first" >&2; exit 2; }
command -v "$chrome" >/dev/null 2>&1 || { echo "error: browser '$chrome' not found" >&2; exit 2; }

profile=$(mktemp -d)
log=$(mktemp)
python3 -m http.server "$port" --directory "$build_dir/bin" --bind 127.0.0.1 >/dev/null 2>&1 &
server_pid=$!
cleanup() {
  kill "$server_pid" 2>/dev/null || true
  rm -rf "$profile"
  rm -f "$log"
}
trap cleanup EXIT

for _ in $(seq 1 50); do
  if curl -fsS -o /dev/null "http://127.0.0.1:$port/cna-3d-museum.html" 2>/dev/null; then break; fi
  sleep 0.1
done

# A server that could not bind (the port is taken) must not let the browser load somebody else's page.
kill -0 "$server_pid" 2>/dev/null || { echo "error: the HTTP server did not start on port $port (MUSEUM_WEB_PORT)" >&2; exit 2; }

timeout 45 "$chrome" --headless=new --no-sandbox --disable-gpu-sandbox \
  --enable-webgl --enable-unsafe-swiftshader --use-angle=swiftshader \
  --user-data-dir="$profile" --enable-logging=stderr --v=1 \
  "http://127.0.0.1:$port/cna-3d-museum.html?smoke=1" >"$log" 2>&1 || true

grep -aE 'CONSOLE.*MUSEUM_(OUT|ERR|ABORT|EXIT|PAGE_ERROR)' "$log" | \
  sed -E 's/^.*CONSOLE[^\"]*\"//; s/\", source.*$//' || true

if grep -aq 'MUSEUM_EXIT 0' "$log" &&
   grep -aq '\[museum\]\[stats\].*exhibit errors 0' "$log" &&
   grep -aq '\[museum\]\[progress\] autosaved .* after 5\.[01] idle seconds' "$log" &&
   ! grep -aqE 'MUSEUM_(ABORT|PAGE_ERROR)|MUSEUM_EXIT [1-9]|WebGL context.*lost|\[museum\]\[(exhibit|draw|script)-error\]' "$log"; then
  echo "WEBGL2 BROWSER SMOKE: PASS"
  BUILD_DIR=$build_dir python3 tools/record_profile.py web-webgl2 --passed 1 --failed 0 --binary "$build_dir/bin/cna-3d-museum.wasm" --library "$build_dir/bin/cna-3d-museum.wasm" || true
else
  echo "WEBGL2 BROWSER SMOKE: FAIL" >&2
  BUILD_DIR=$build_dir python3 tools/record_profile.py web-webgl2 --passed 0 --failed 1 --binary "$build_dir/bin/cna-3d-museum.wasm" --library "$build_dir/bin/cna-3d-museum.wasm" || true
  tail -n 80 "$log" >&2
  exit 1
fi
