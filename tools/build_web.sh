#!/usr/bin/env bash
# Build the browser/WebAssembly edition with CNA's WebGL 2 renderer and, chosen at run time (?renderer=WEBGPU), its
# browser WebGPU renderer (Emscripten's emdawnwebgpu port from the emsdk cache). MUSEUM_WEB_RENDERERS overrides the list.
# Reuses build-consumer/, the project's shared cross-compilation build tree.
set -euo pipefail

cd "$(dirname "$0")/.."
build_dir=${BUILD_DIR:-build-consumer}
jobs=${JOBS:-2}
emsdk_root=${EMSDK_ROOT:-${EMSDK:-/home/robertvokac/emsdk}}

case $jobs in
  1|2) ;;
  *) echo "error: JOBS must be 1 or 2" >&2; exit 2 ;;
esac
if [ "$build_dir" != build-consumer ]; then
  echo "error: the web build uses the shared build-consumer directory" >&2
  exit 2
fi
if [ ! -x "$emsdk_root/upstream/emscripten/emcmake" ]; then
  echo "error: Emscripten SDK not found at $emsdk_root (set EMSDK_ROOT)" >&2
  exit 2
fi
if ! command -v ccache >/dev/null 2>&1; then
  echo "error: ccache is required" >&2
  exit 2
fi

export CCACHE_DIR=${CCACHE_DIR:-/rv/cnaccache}
export CCACHE_BASEDIR=${CCACHE_BASEDIR:-/rv}
export EMSDK="$emsdk_root"

fresh=()
if [ -f "$build_dir/CMakeCache.txt" ] &&
   ! grep -q '^EMSCRIPTEN:INTERNAL=1$' "$build_dir/CMakeCache.txt"; then
  fresh=(--fresh)
fi

"$emsdk_root/upstream/emscripten/emcmake" cmake "${fresh[@]}" -S . -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
  -DCNA_GRAPHICS_RENDERER=WEBGL2 \
  "-DCNA_GRAPHICS_RENDERERS=${MUSEUM_WEB_RENDERERS:-WEBGL2;WEBGPU}" \
  -DCNA_ENABLE_EMSCRIPTEN_THREADS=OFF \
  -DMUSEUM_NETWORKING=OFF \
  -DMUSEUM_INSPECTOR=OFF \
  -DMUSEUM_VIDEO=OFF \
  -DMUSEUM_C_API=OFF

cmake --build "$build_dir" --target cna-3d-museum --parallel "$jobs"

for suffix in html js wasm data; do
  artifact="$build_dir/bin/cna-3d-museum.$suffix"
  [ -s "$artifact" ] || { echo "error: missing web artifact $artifact" >&2; exit 1; }
done
echo "web build ok: $build_dir/bin/cna-3d-museum.html"
