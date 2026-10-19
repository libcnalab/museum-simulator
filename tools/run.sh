#!/usr/bin/env bash
# Runs the museum on a private virtual X display with a test script and returns the museum's exit status.
#   tools/run.sh "pos:...;look:...;shot:out.png;quit" [museum options...]
#   tools/run.sh @script.txt [museum options...]      the script from a file (a long tour does not fit in one argument)
# MUSEUM_LOG: where the full log goes (default build/run.log); a second run on the same log is refused.
# See MuseumGame::RunScript (src/Testing/ScriptRunner.cpp) for the script language.
set -u
source "$(dirname "$0")/_headless.sh"
[ $# -ge 1 ] || { echo "usage: tools/run.sh \"script\" [museum options...]" >&2; exit 2; }
script=$1; shift
root=$(cd "$(dirname "$0")/.." && pwd)
log=${MUSEUM_LOG:-$root/${BUILD_DIR:-build}/run.log}
if [ "${script:0:1}" = "@" ]; then
  museum_headless "$log" --script-file "${script:1}" --fixed-step 0.02 "$@"
else
  museum_headless "$log" --script "$script" --fixed-step 0.02 "$@"
fi
exit $?
