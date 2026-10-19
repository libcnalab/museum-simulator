#!/usr/bin/env bash
# Compares the arrival view of every room (or the ones named) on one renderer with the same view on another and ranks the rooms by
# how many pixels differ. A room that differs a lot is either a renderer divergence worth a look or an animation that did not
# keep time; the museum clock, the step and the frame count are fixed so that two runs on the same renderer agree.
#   tools/room_diff.sh RENDERER [-b BASE_RENDERER] [-t PERCENT] [-c] [room-key ...]
# `-c` compares existing pictures without rendering them again; base defaults to OPENGLES3.
# Needs both renderers in the same build (MUSEUM_RENDERERS) and Pillow. Pictures go to ${BUILD_DIR}/room_diff/<RENDERER>/.
set -u
cd "$(dirname "$0")/.." || exit 2
[ $# -ge 1 ] || { echo "usage: tools/room_diff.sh RENDERER [-b BASE] [-t PERCENT] [-c] [room-key ...]" >&2; exit 2; }
renderer=$1; shift
base=OPENGLES3
threshold=2.0
compare_only=0
while [ $# -gt 0 ]; do
  case "$1" in
    -b) base=$2; shift 2 ;;
    -t) threshold=$2; shift 2 ;;
    -c) compare_only=1; shift ;;
    *) break ;;
  esac
done
bin=${MUSEUM_BIN:-${BUILD_DIR:-build}/bin/cna-3d-museum}
if [ $# -gt 0 ]; then keys=("$@"); else mapfile -t keys < <("$bin" --list-rooms); fi
out=${BUILD_DIR:-build}/room_diff
run_failed=0
if [ "$compare_only" -eq 0 ]; then
  for r in "$base" "$renderer"; do
    mkdir -p "$out/$r"
    # Pictures from an earlier run must not stand in for rooms this run failed to reach.
    for k in "${keys[@]}"; do rm -f "$out/$r/$k.png"; done
    script=""
    for k in "${keys[@]}"; do script+="goto:$k;wait:8;shot:$PWD/$out/$r/$k.png;"; done
    CNA_GRAPHICS_RENDERER=$r MUSEUM_TIMEOUT=${MUSEUM_TIMEOUT:-1500} tools/run.sh "${script}quit" --no-audio --no-hud --time 11 --width 640 --height 360 \
      > "$out/$r.log" 2>&1 || { echo "FAIL: $r exited with status $? (see $out/$r.log); its missing rooms are listed as 100 %"; run_failed=1; }
  done
fi
python3 -I - "$out/$base" "$out/$renderer" "$threshold" "${keys[@]}" <<'PY'
import sys
from PIL import Image, ImageChops
a, b, thr = sys.argv[1], sys.argv[2], float(sys.argv[3])
rows = []
for k in sys.argv[4:]:
    try:
        x = Image.open(f"{a}/{k}.png").convert("RGB"); y = Image.open(f"{b}/{k}.png").convert("RGB")
        d = ImageChops.difference(x, y).convert("L")
        # Pillow < 14 exposes the flattened pixel iterator as getdata(); Pillow 14 renamed it to
        # get_flattened_data(). Keep this developer tool usable with both distro and current Pillow.
        pixels = d.get_flattened_data() if hasattr(d, "get_flattened_data") else d.getdata()
        n = sum(1 for p in pixels if p > 24)
        rows.append((100.0 * n / (x.width * x.height), k))
    except Exception as e:
        rows.append((100.0, k + " (missing: %s)" % e))
rows.sort(reverse=True)
over = [r for r in rows if r[0] > thr]
print(f"{len(rows)} rooms compared, {len(over)} differ by more than {thr}% of pixels")
for pct, k in over:
    print(f"  {pct:6.2f} %  {k}")
sys.exit(1 if over else 0)
PY
compare=$?
[ "$run_failed" = 1 ] && exit 1
exit $compare
