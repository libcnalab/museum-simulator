#!/usr/bin/env bash
# Runs a command on a private virtual X display, in place of `xvfb-run -a`. Xvfb picks a free display number itself
# (-displayfd), so concurrent runs from other sessions on this machine can never claim the same number, and nothing is kept
# under /tmp (xvfb-run's auth directory there was removed under a running tour by another session's cleanup).
#   tools/private_xvfb.sh [-s "SERVER ARGS"] [-e SERVER-LOG] COMMAND [ARGS...]     exit status: COMMAND's
set -u
server="-screen 0 1280x720x24" errors=/dev/null
while [ $# -gt 0 ]; do
  case $1 in
    -s) server=$2; shift 2 ;;
    -e) errors=$2; shift 2 ;;
    *) break ;;
  esac
done
[ $# -gt 0 ] || { echo "usage: $0 [-s SERVER-ARGS] [-e SERVER-LOG] COMMAND [ARGS...]" >&2; exit 2; }
numberfile=$(mktemp -p "${XDG_RUNTIME_DIR:-/tmp}" museum-xvfb.XXXXXX) || exit 2
# shellcheck disable=SC2086
Xvfb -displayfd 3 -nolisten tcp $server 3> "$numberfile" 2> "$errors" &
xvfb=$!
for _ in $(seq 1 300); do [ -s "$numberfile" ] && break; kill -0 $xvfb 2> /dev/null || break; sleep 0.05; done
number=$(tr -dc '0-9' < "$numberfile")
rm -f "$numberfile"
[ -n "$number" ] || { echo "error: Xvfb did not start (log: $errors)" >&2; kill $xvfb 2> /dev/null; exit 2; }
trap 'kill $xvfb 2> /dev/null; wait $xvfb 2> /dev/null' EXIT
# A time limit (timeout) signals this script: pass it on, so the command does not outlive its display.
DISPLAY=:$number "$@" &
child=$!
trap 'kill -TERM $child 2> /dev/null' TERM INT
wait $child
status=$?
# wait returns early when a signal arrives; collect the command's own status.
kill -0 $child 2> /dev/null && { wait $child; status=$?; }
exit $status
