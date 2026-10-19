#!/usr/bin/env bash
# End-to-end check of the Online rooms against a private CNA gamer-services server (tools/online_stack.sh): starts the service from
# scratch, runs the museum on a private virtual display, types the Visitor account's name and password into the Guide with xdotool,
# plays one scored round in the Leaderboards room, and then looks at the service's own database for what the museum sent
# (the account's rating and its first achievement). Needs Xvfb, xdotool, sqlite3 and the built service.
#   tools/online_check.sh        exit 0 only if every step passed
#
# Exit status: 0 passed, 1 failed, 77 skipped because a prerequisite is absent (a missing tool or an unbuilt service). A service
# that is built but does not start is a failure, not a skip. The check runs its own stack in build/online-check on its own port
# (MUSEUM_ONLINE_PORT, default 47832), so it never wipes or stops an interactive stack in build/online.
set -u
cd "$(dirname "$0")/.." || exit 2
for tool in Xvfb xdotool sqlite3; do command -v $tool > /dev/null || { echo "skipped: $tool is not installed"; exit 77; }; done
export MUSEUM_ONLINE_DIR=$PWD/${BUILD_DIR:-build}/online-check MUSEUM_ONLINE_PORT=${MUSEUM_ONLINE_PORT:-47832}
dir=$MUSEUM_ONLINE_DIR
bin=${MUSEUM_BIN:-${BUILD_DIR:-build}/bin/cna-3d-museum}
[ -x "$bin" ] || { echo "FAIL: $bin is not built"; exit 1; }
mkdir -p "$dir"
museum= xvfb=
cleanup() {
  [ -n "$museum" ] && kill "$museum" 2> /dev/null
  [ -n "$xvfb" ] && kill "$xvfb" 2> /dev/null
  tools/online_stack.sh stop > /dev/null
}
trap cleanup EXIT
tools/online_stack.sh stop > /dev/null
tools/online_stack.sh start --fresh > "$dir/start.log" 2>&1
case $? in
  0) ;;
  3) echo "skipped: the service is not built (see tools/online_stack.sh)"; exit 77 ;;
  *) echo "FAIL: the service is built but did not start: $(tail -1 "$dir/start.log")"; exit 1 ;;
esac
eval "$(tools/online_stack.sh env)"
export CNA_GAMER_SERVICES_PROFILES_DIR=$dir/check-profiles CNA_GAMER_SERVICES_CREDENTIALS_DIR=$dir/check-credentials SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=x11 LIBGL_ALWAYS_SOFTWARE=1
export LP_NUM_THREADS=${LP_NUM_THREADS:-${MUSEUM_RENDER_THREADS:-3}}
# SDL prefers a live Wayland display over DISPLAY when both are inherited. This check must stay on
# its private Xvfb display so xdotool targets the Guide window it just opened.
unset WAYLAND_DISPLAY
rm -rf "$CNA_GAMER_SERVICES_PROFILES_DIR" "$CNA_GAMER_SERVICES_CREDENTIALS_DIR"
mkdir -m 700 -p "$CNA_GAMER_SERVICES_CREDENTIALS_DIR"
log=$dir/check.log
display=$((300 + RANDOM % 50))
Xvfb :$display -screen 0 1920x1080x24 > /dev/null 2>&1 & xvfb=$!
export DISPLAY=:$display
sleep 1
kill -0 $xvfb 2> /dev/null || { xvfb=; echo "FAIL: Xvfb :$display did not start"; exit 1; }
script="goto:gs-leaderboards;wait:20;e;wait:20;press:Guide.ShowSignIn=1;wait:1200;press:add 10 to=1;wait:900;goto:gs-players;wait:200;quit"
timeout -k 15 "${MUSEUM_TIMEOUT:-300}" "$bin" --script "$script" --fixed-step 0.02 --no-audio > "$log" 2>&1 & museum=$!
fail() { echo "FAIL: $1 (log: $log)"; exit 1; }
wait_for() { for _ in $(seq 1 240); do grep -q "$1" "$log" && return 0; kill -0 $museum 2> /dev/null || return 1; sleep 0.5; done; return 1; }
wait_for "ShowSignIn(1, true) opened" || fail "the sign-in page never opened"
sleep 2
xdotool windowfocus "$(xdotool search --onlyvisible --name '' | tail -1)" 2> /dev/null
xdotool key Return; sleep 1.5
xdotool type --delay 60 visitor; xdotool key Return; sleep 4
xdotool type --delay 60 museum-visitor-pass; xdotool key Return
wait_for "round played" || fail "no scored round was played (sign-in failed?): $(grep 'leaderboard write' "$log" | head -1)"
wait $museum; status=$?; museum=
[ $status = 0 ] || fail "the museum exited with status $status"
grep -q "\[museum\]\[exhibit-error\]" "$log" && fail "an exhibit threw during the check"
rating=$(sqlite3 "$dir/service.sqlite3" "select e.rating from leaderboard_entries e join users u on u.id=e.user_id where u.gamertag='Visitor'")
earned=$(sqlite3 "$dir/service.sqlite3" "select count(*) from earned e join users u on u.id=e.user_id where u.gamertag='Visitor'")
[ "$rating" = 10 ] || fail "the service holds rating '${rating:-none}' for Visitor, expected 10"
[ "${earned:-0}" -ge 1 ] || fail "the service holds no achievement for Visitor"
echo "ok: signed in through the Guide, rated 10 on the service's board, $earned achievement(s) earned"
