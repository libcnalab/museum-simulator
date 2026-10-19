#!/usr/bin/env bash
# The whole verification battery on the default renderer (a few minutes). Prints one line per step and exits non-zero
# if any step failed. SLOW=1 also tours, self-checks and leak-checks OPENGL33 and SOFTWARE (SOFTWARE is slow).
#   tools/check_all.sh
set -u
cd "$(dirname "$0")/.." || exit 2
failed=0
step() {   # step NAME COMMAND...
  local name=$1; shift
  if "$@" > "${BUILD_DIR:-build}/check_$(echo "$name" | tr ' /' '__').log" 2>&1; then echo "ok    $name"; else echo "FAIL  $name  (log: ${BUILD_DIR:-build}/check_$(echo "$name" | tr ' /' '__').log)"; failed=1; fi
}
step "build" tools/build.sh cna-3d-museum museum_tests
step "museum_tests" "${BUILD_DIR:-build}/bin/museum_tests"
step "validate" "${BUILD_DIR:-build}/bin/cna-3d-museum" --validate
step "generated documentation is current" tools/gen_docs.sh --check
step "public API inventory is current" python3 tools/generate_api_inventory.py --check
step "coverage evidence is current" python3 tools/audit_api_coverage.py --check
step "tour every room, operate every control" tools/tour.sh -f 3 -p
step "elevator to every floor" tools/nav_test.sh
step "renderer self-checks" tools/renderer_check.sh OPENGLES3
step "visual regression" tools/reference.sh compare
step "memory over three tours" tools/leak_check.sh
if [ "${ONLINE:-0}" = 1 ]; then   # needs Xvfb, xdotool, sqlite3 and the built service (tools/online_stack.sh); exit 77 = skipped
  step "online service (sign-in, scored round, achievement)" bash -c 'tools/online_check.sh; rc=$?; [ $rc = 77 ] && exit 0; exit $rc'
fi
if [ "${SLOW:-0}" = 1 ]; then
  for r in OPENGL33 SOFTWARE; do
    step "tour on $r" env CNA_GRAPHICS_RENDERER=$r MUSEUM_TIMEOUT=2400 tools/tour.sh -f 3 -p
    step "self-checks on $r" tools/renderer_check.sh $r
    step "memory on $r" env MUSEUM_TIMEOUT=2400 tools/leak_check.sh $r
  done
fi
exit $failed
