# Shared step accounting, sourced by check_all.sh (and its unit test).
#
# step NAME COMMAND...   runs COMMAND with its output in ${BUILD_DIR:-build}/check_<name>.log and prints one line:
#                        "ok" (exit 0), "skip" (exit 77, with the first line of the log that names the skip) or "FAIL".
#                        Returns the command's own status.
# required_skip NAME     the step that just skipped was required by the profile: it becomes a failure.
# summary                prints passed / failed / skipped / not-run counts; a skip is never counted as passed.
# notrun+=("what")       records a part of the battery the chosen profile did not run.
failed=0
passed=0
failures=0
skipped=0
notrun=()
step() {
  local name=$1; shift
  local log="${BUILD_DIR:-build}/check_$(echo "$name" | tr ' /' '__').log"
  "$@" > "$log" 2>&1
  local rc=$?
  if [ $rc = 0 ]; then
    echo "ok    $name"; passed=$((passed + 1))
  elif [ $rc = 77 ]; then
    echo "skip  $name  ($(grep -m1 -i 'skip' "$log" | cut -c1-120); log: $log)"; skipped=$((skipped + 1))
  else
    echo "FAIL  $name  (log: $log)"; failed=1; failures=$((failures + 1))
  fi
  return $rc
}
required_skip() {
  echo "FAIL  $1: the profile requires it, but it was skipped"
  skipped=$((skipped - 1)); failures=$((failures + 1)); failed=1
}
summary() {
  local extra=""
  [ ${#notrun[@]} -gt 0 ] && extra=" ($(IFS=';'; echo "${notrun[*]}"))"
  echo "summary: $passed passed, $failures failed, $skipped skipped, ${#notrun[@]} not run$extra"
}
