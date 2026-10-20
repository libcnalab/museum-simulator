#!/usr/bin/env bash
# Visual regression for the museum's reference views (src/Catalogue/ReferenceViews.cpp).
#
#   tools/reference.sh capture [VIEW ...]    render the views and store them as the reference images
#   tools/reference.sh compare [VIEW ...]    render them again and report how much each differs
#
# Images live in tests/reference/<RENDERER>/<VIEW>.png (640x360) and are rendered with the museum clock fixed at
# 11:00, a fixed time step, no audio and no HUD, so a run on the same renderer is repeatable. Renderers differ a
# little from each other (filtering, rasterisation), therefore each has its own reference set.
#
# Environment: CNA_GRAPHICS_RENDERER (default OPENGLES3), THRESHOLD (percent of pixels allowed to
# differ by more than 24/255, default 1.0), BUILD_DIR.
# Exit status: 0 when every compared view is within THRESHOLD; 1 otherwise.
set -u
cd "$(dirname "$0")/.." || exit 2
mode=${1:-compare}; shift || true
bin=${MUSEUM_BIN:-${BUILD_DIR:-build}/bin/cna-3d-museum}
renderer=${CNA_GRAPHICS_RENDERER:-OPENGLES3}
export CNA_GRAPHICS_RENDERER=$renderer
refdir=tests/reference/$renderer
outdir=${BUILD_DIR:-build}/reference/$renderer
threshold=${THRESHOLD:-1.0}
mkdir -p "$outdir"
views=("$@")
if [ ${#views[@]} -eq 0 ]; then
  while IFS=$'\t' read -r name _; do views+=("$name"); done < <("$bin" --list-views)
fi
[ "$mode" = capture ] && mkdir -p "$refdir"
status=0
for v in "${views[@]}"; do
  rm -f "$outdir/$v.png"   # an image left from an earlier run must never be compared as this run's
  tools/shot.sh "$outdir/$v.png" --view "$v" --no-hud --time 11 --no-audio --width 640 --height 360 --fixed-step 0.02 --screenshot-frame 20 > "$outdir/$v.log" 2>&1 || { echo "FAIL  $v: the museum did not produce a screenshot (see $outdir/$v.log)"; status=1; continue; }
  if [ ! -s "$outdir/$v.png" ] || grep -qE '\[museum\]\[(exhibit|draw|script)-error\]' "$outdir/$v.log"; then
    echo "FAIL  $v: no fresh screenshot, or an error while rendering it ($(grep -m1 -E 'error\]' "$outdir/$v.log" | cut -c1-120); log $outdir/$v.log)"; status=1; continue
  fi
  if [ "$mode" = capture ]; then
    # The museum's own PNG writer stores uncompressed data; recompress for the repository (pixels are unchanged).
    python3 -I -c "import sys; from PIL import Image; Image.open(sys.argv[1]).convert('RGB').save(sys.argv[2], optimize=True, compress_level=9)" "$outdir/$v.png" "$refdir/$v.png"
    echo "stored  $refdir/$v.png ($(( $(stat -c %s "$refdir/$v.png") / 1024 )) KB)"
  else
    if [ ! -f "$refdir/$v.png" ]; then echo "MISSING $v: no reference image in $refdir (run: tools/reference.sh capture)"; status=1; continue; fi
    pct=$(python3 -I - "$refdir/$v.png" "$outdir/$v.png" <<'PY'
import sys
from PIL import Image, ImageChops
a = Image.open(sys.argv[1]).convert("RGB"); b = Image.open(sys.argv[2]).convert("RGB")
if a.size != b.size:
    print("100.00"); sys.exit()
d = ImageChops.difference(a, b).convert("L")
px = d.tobytes()
print("%.2f" % (100.0 * sum(1 for v in px if v > 24) / len(px)))
PY
)
    if python3 -I -c "import sys; sys.exit(0 if float('$pct') <= float('$threshold') else 1)"; then echo "ok    $v  ($pct % differ)"; else echo "DIFF  $v  ($pct % differ, limit $threshold)"; status=1; fi
  fi
done
exit $status
