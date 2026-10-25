#!/usr/bin/env bash
# Build the Android edition (android/, SDL's Java glue + the museum as libmain.so, OPENGLES3) as a debug APK.
#   tools/build_android.sh          ABIs from MUSEUM_ANDROID_ABIS (default x86_64, the emulator's); "x86_64,arm64-v8a" for both
# Output stays in the shared build-consumer/ tree (android-cxx/, android-gradle/, android-assets/). Gradle runs offline
# from its local caches, and the CMake job pool holds the native build to eight jobs.
set -euo pipefail

cd "$(dirname "$0")/.."
sdk=${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}}
[ -d "$sdk/ndk/29.0.14206865" ] || { echo "error: NDK 29.0.14206865 not found under $sdk/ndk (set ANDROID_HOME)" >&2; exit 2; }
command -v ccache >/dev/null 2>&1 || { echo "error: ccache is required" >&2; exit 2; }
command -v java >/dev/null 2>&1 || { echo "error: a JDK (17 or newer) is required" >&2; exit 2; }

export ANDROID_HOME=$sdk
export CCACHE_DIR=${CCACHE_DIR:-/rv/cnaccache}
export CCACHE_BASEDIR=${CCACHE_BASEDIR:-/rv}
mkdir -p build-consumer
log=build-consumer/android-build.log
(cd android && ./gradlew --offline --no-daemon --console=plain --project-cache-dir ../build-consumer/android-gradle-cache \
  -PcnaRootDir="$(cd ../../cna && pwd)" -PmuseumAbis="${MUSEUM_ANDROID_ABIS:-x86_64}" \
  ${MUSEUM_ANDROID_CMAKE_ARGS:+-PmuseumCmakeArgs="$MUSEUM_ANDROID_CMAKE_ARGS"} assembleDebug) > "$log" 2>&1 || {
  grep -E 'error:|FAILED|What went wrong' -A3 "$log" | head -40
  echo "android build FAILED (log: $log)"; exit 1; }
apk=build-consumer/android-gradle/outputs/apk/debug/app-debug.apk
[ -s "$apk" ] || { echo "error: $apk was not produced (log: $log)" >&2; exit 1; }
echo "android build ok: $apk ($(du -h "$apk" | cut -f1))"
