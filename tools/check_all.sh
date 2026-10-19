#!/usr/bin/env bash
# The whole verification battery on the default renderer (a few minutes). Prints one line per step and exits non-zero
# if any step failed. SLOW=1 also tours, self-checks and leak-checks OPENGL33 and SOFTWARE (SOFTWARE is slow).
# ONLINE=1 requires the online service check: a skip (exit 77: no Xvfb/xdotool/sqlite3 or the service is not built) then
# counts as a failure, because the profile that asked for it was not verified. Without ONLINE the online check is not run
# and the summary lists it as not run.
#   tools/check_all.sh
# The last line is a summary: passed, failed, skipped and not-run steps. A skip is never reported as ok.
set -u
cd "$(dirname "$0")/.." || exit 2
source tools/_steps.sh
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
if [ "${ONLINE:-0}" = 1 ]; then   # needs Xvfb, xdotool, sqlite3 and the built service (tools/online_stack.sh)
  step "online service (sign-in, scored round, achievement)" tools/online_check.sh
  [ $? = 77 ] && required_skip "online service"
else
  notrun+=("online service (ONLINE=1)")
fi
if [ "${SLOW:-0}" = 1 ]; then
  for r in OPENGL33 SOFTWARE; do
    step "tour on $r" env CNA_GRAPHICS_RENDERER=$r MUSEUM_TIMEOUT=2400 tools/tour.sh -f 3 -p
    step "self-checks on $r" tools/renderer_check.sh $r
    step "memory on $r" env MUSEUM_TIMEOUT=2400 tools/leak_check.sh $r
  done
else
  notrun+=("OPENGL33 and SOFTWARE tours, self-checks and leak checks (SLOW=1)")
fi
summary
exit $failed
