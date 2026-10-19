#!/usr/bin/env bash
# Navigation smoke test: rides the elevator to every floor, then physically walks every stair flight upward and
# the first flight downward. It checks the museum's own statistics line after each route.
set -u
cd "$(dirname "$0")/.." || exit 2
bin=${MUSEUM_BIN:-${BUILD_DIR:-build}/bin/cna-3d-museum}
floors=$("$bin" --inventory | grep -o '"floors": [0-9]*' | grep -o '[0-9]*')
status=0
# A route only counts when its run reached the stats step with no caught exhibit, draw or script error.
clean() {
  local log="${BUILD_DIR:-build}/run.log"
  grep -q "\[museum\]\[stats\].* exhibit errors 0\b" "$log" && ! grep -q "\[museum\]\[\(exhibit\|draw\|script\)-error\]" "$log"
}
for ((f = 0; f < floors; ++f)); do
  tools/run.sh "ride:$f;wait:160;stats;directory:open;wait:5;directory:close;wait:3;quit" --no-audio > "${BUILD_DIR:-build}/nav.out" 2>&1 || { echo "FAIL floor $f: the run failed"; status=1; continue; }
  got=$(grep -o " floor [0-9]*  exhibit errors" "${BUILD_DIR:-build}/run.log" | tail -1 | grep -o "floor [0-9]*" | grep -o '[0-9]*')
  if [ "$got" = "$f" ] && clean; then echo "ok    ride to floor $f"; else echo "FAIL  ride to floor $f arrived on floor ${got:-?} (or an error was logged: ${BUILD_DIR:-build}/run.log)"; status=1; fi
done

# Enter each stair from its lower-floor hall, walk the complete switchback, and
# leave through the upper-floor hall.  Starting inside the stair would miss a
# blocked doorway or a landing that a real visitor cannot enter.
for ((f = 0; f + 1 < floors; ++f)); do
  upper=$((f + 1))
  if ((f % 2 == 0)); then door_z=-2.9; else door_z=6.1; fi
  tools/run.sh "floor:$f;wait:8;at:-9,$door_z,90,0;wait:3;walk:1,0,210;look:0,0;walk:1,0,55;look:-90,0;walk:1,0,210;wait:8;stats;quit" --no-audio > "${BUILD_DIR:-build}/nav.out" 2>&1 || { echo "FAIL stairs $f->$upper: the run failed"; status=1; continue; }
  got=$(grep -o " floor [0-9]*  exhibit errors" "${BUILD_DIR:-build}/run.log" | tail -1 | grep -o "floor [0-9]*" | grep -o '[0-9]*')
  visited=$(grep -o " visited [0-9]*  floor" "${BUILD_DIR:-build}/run.log" | tail -1 | grep -o '[0-9]*')
  if [ "$got" = "$upper" ] && [ "${visited:-0}" -ge 3 ] && clean; then echo "ok    stairs from floor $f hall to floor $upper hall"; else echo "FAIL  stairs from floor $f to $upper arrived on floor ${got:-?} after visiting ${visited:-?} cells"; status=1; fi
done

# Reverse traversal starts in the upper hall too, exercising both doorways as
# well as descending ground resolution.
if tools/run.sh 'floor:1;wait:8;at:-9,-6.1,90,0;wait:3;walk:1,0,210;look:180,0;walk:1,0,55;look:-90,0;walk:1,0,210;wait:8;stats;quit' --no-audio > "${BUILD_DIR:-build}/nav.out" 2>&1; then
  got=$(grep -o " floor [0-9]*  exhibit errors" "${BUILD_DIR:-build}/run.log" | tail -1 | grep -o "floor [0-9]*" | grep -o '[0-9]*')
  visited=$(grep -o " visited [0-9]*  floor" "${BUILD_DIR:-build}/run.log" | tail -1 | grep -o '[0-9]*')
  if [ "$got" = 0 ] && [ "${visited:-0}" -ge 3 ] && clean; then echo "ok    stairs from floor 1 hall to floor 0 hall"; else echo "FAIL  stairs from floor 1 to 0 arrived on floor ${got:-?} after visiting ${visited:-?} cells"; status=1; fi
else
  echo "FAIL stairs 1->0: the run failed"
  status=1
fi
exit $status
